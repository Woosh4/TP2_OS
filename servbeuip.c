/* Exemple de client UDP user datagram protocol 
socket en mode non connecté */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#include <pthread.h>

#include <ifaddrs.h>
#include <netdb.h>

#include "servbeuip.h"

/* ---- vraiables globales ----*/

struct personne table[TABLE_TAILLE];
int table_wr = 0;
pthread_mutex_t mutex_annuaire = PTHREAD_MUTEX_INITIALIZER;

int sid;
int trace = 0;

void init_serveur() {
    struct sockaddr_in serveur_sock;
    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        exit(2);
    }

    bzero(&serveur_sock, sizeof(serveur_sock));
    serveur_sock.sin_family = AF_INET;
    serveur_sock.sin_port = htons(PORT);
    serveur_sock.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sid, (struct sockaddr*)&serveur_sock, sizeof(serveur_sock)) == -1){
        perror("bind");
        exit(3);
    }
    printf("Le serveur est attaché au port %d\n", PORT);
}

void envoi_msg_connexion(const char* mon_pseudo) {
    char buf[LBUF+1];
    sprintf(buf, "1BEUIP%s", mon_pseudo);
    
    if(trace) printf("buf pour envoi connexion = %s\n", buf);

    diffuser_broadcast_dynamique(sid, buf, PORT);
}

void affiche_liste(){
    printf("----- ANNUAIRE -----\n");
    printf("-- PERSONNE -- ADRESSE -- \n");
    pthread_mutex_lock(&mutex_annuaire);
    for(int i=0; i<table_wr; ++i){
        printf("%s --- %s\n", table[i].pseudo, inet_ntoa(table[i].adresse_ip.sin_addr));
    }
    pthread_mutex_unlock(&mutex_annuaire);
}

void ajouter_personne(const char* pseudo, struct sockaddr_in client_sock) {
    if(table_wr >= TABLE_TAILLE) return;
    
    int connu = 0;
    pthread_mutex_lock(&mutex_annuaire);
    for(int i=0; i<table_wr; ++i){
        if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
            connu = 1;
            if(trace) printf("Personne connue: %s\n", table[i].pseudo);
            break;
        }
    }
    if(!connu){
        strcpy(table[table_wr].pseudo, pseudo);
        table[table_wr].adresse_ip = client_sock;
        if(trace) printf("Nouvel utilisateur : numéro %d, nom: %s, adresse %s\n", table_wr, table[table_wr].pseudo, inet_ntoa(client_sock.sin_addr));
        ++table_wr;
    }
    pthread_mutex_unlock(&mutex_annuaire);
}

void retirer_personne(struct sockaddr_in client_sock) {
    pthread_mutex_lock(&mutex_annuaire);
    for(int i=0; i<table_wr; ++i){
        if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
            if(trace) printf("%s s'est déconnecté.\n", table[i].pseudo);
            memset(&table[i].pseudo, '\0', sizeof(table[i].pseudo)); 
            memset(&table[i].adresse_ip, 0, sizeof(table[i].adresse_ip)); 
            break; // Une seule adresse correspondante
        }
    }
    pthread_mutex_unlock(&mutex_annuaire);
}

void repondre_ack(struct sockaddr_in client_sock, const char* mon_pseudo) {
    char ack_msg[LBUF];
    sprintf(ack_msg, "2BEUIP%s", mon_pseudo);
    if(sendto(sid, ack_msg, strlen(ack_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sizeof(client_sock)) == -1){
        perror("sendto ack");
    }
}

void gerer_reception_prive(const char* buf, struct sockaddr_in client_sock) {
    pthread_mutex_lock(&mutex_annuaire);
    for(int i=0; i<table_wr; ++i){
        if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr && table[i].pseudo[0] != '\0'){
            printf("Message de %s : %s", table[i].pseudo, &buf[6]);
            pthread_mutex_unlock(&mutex_annuaire);
            return;
        }
    }
    pthread_mutex_unlock(&mutex_annuaire);
    printf("ERREUR: expéditeur du message privé non trouvé dans l'annuaire\n");
}

void traiter_message_recu(char* buf, int ret, struct sockaddr_in client_sock, const char* mon_pseudo) {
    buf[ret] = '\0';
    char code = buf[0];

    // ack si nouvelle connexion ou tout autre message qui n'est pas un ack
    // sauf messages locaux et messages reçus de moi même par le broadcast : pas de ack
    if(code == '1' && strcmp(&buf[6], mon_pseudo) != 0){
        repondre_ack(client_sock, mon_pseudo);
    }

    // màj de l'annuaire si le message est correct
    if(strncmp(&buf[1], "BEUIP", 5) == 0){
        ajouter_personne(&buf[6], client_sock);
    }

    // choix de l'action suivant le code
    switch(code){
        case '0':
            retirer_personne(client_sock);
            break;
        case '1':
            // géré par ajouter_personne
            break;
        case '2':
            // ack, rien à faire
            break;
        case '9':
            gerer_reception_prive(buf, client_sock);
            break;
        default:
            fprintf(stderr, "\nTentative de piratage ou paquet corrompu! code reçu : %c\n", code);
            break;
    }
    // séparation pour rendre plus lisible
    printf("\n- - - - - - - - - -\n");
}

void cleanup_serveur(void *arg){
    close(sid);
    if(trace) printf("\nArrêt du thread serveur et fermeture du port OK.\n");
}

void* serveur_udp(void* p){
    //pour les codes retours
    int ret;
    char buf[LBUF+1];
    struct sockaddr_in client_sock;
    char* mon_pseudo = (char*)p;

    init_serveur();
    envoi_msg_connexion(mon_pseudo);

    // pour gérer le cleanup à fin du thread
    pthread_cleanup_push(cleanup_serveur, NULL);

    /* Lecture des messages */
    if(trace) printf("Ctrl+C pour quitter, début d'écoute.\n");
    do {
        socklen_t sockaddr_taille = sizeof(client_sock);
        if((ret = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr*)&client_sock, &sockaddr_taille)) < 0){
            perror("recvfrom");
            break;
        }

        traiter_message_recu(buf, ret, client_sock, mon_pseudo);
    } while(1);
    
    pthread_cleanup_pop(1);
    return NULL;
}

void diffuser_broadcast_dynamique(int sock_fd, const char* message, int port_dest){
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    int broadcast_enable = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    //récupérer liste des interfaces
    if(getifaddrs(&ifaddr) == -1){
        perror("getifaddrs");
        return;
    }

    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){
        //enlever les interfaces sans adresses ou sans adresses de broadcast
        if(ifa->ifa_addr == NULL || ifa->ifa_broadaddr == NULL){
            continue;
        }
        //ne garder que ipv4
        if(ifa->ifa_addr->sa_family == AF_INET){
            
            //extraction de l'adresse de broadcast sous forme de chaîne ("192.168.1.255")
            int s = getnameinfo(ifa->ifa_broadaddr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            
            if(s == 0){
                //pas localhost
                if(strcmp(host, "127.0.0.1") != 0){
                    
                    struct sockaddr_in bcast_addr;
                    bcast_addr.sin_family = AF_INET;
                    bcast_addr.sin_port = htons(port_dest);
                    bcast_addr.sin_addr.s_addr = inet_addr(host);

                    sendto(sock_fd, message, strlen(message), 0, (struct sockaddr*)&bcast_addr, sizeof(bcast_addr));
                    // printf("Broadcast envoyé sur %s (adresse: %s)\n", ifa->ifa_name, host);
                }
            }
        }
    }
    freeifaddrs(ifaddr); 
}