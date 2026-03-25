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

/* ----- STRUCTURES ----- */

struct personne{
    char pseudo[30];
    struct sockaddr_in adresse_ip;
};

struct message{
    unsigned char code;
    char beuip[6];
    char nom[30];
};

/* -----*/
/* ----- DEFINE ET VARIABLES GLOBALES ----- */
/* -----*/

#define LBUF 256
#define PORT 9998

#define TABLE_TAILLE 255
struct personne table[TABLE_TAILLE];
int table_wr = 0;

int sid;
struct sockaddr_in broadcast_sock;
#define BROADCAST_ADDR "192.168.88.255"

int trace = 0; // pour print debug, activé avec -DTRACE en lançant le programme

/* -----*/
/* ----- FONCTIONS ----- */
/* -----*/

void affiche_liste(){
    printf("----- ANNUAIRE -----\n");
    printf("-- PERSONNE -- ADRESSE -- \n");
    for(int i=0; i<table_wr; ++i){
        printf("%s --- %s\n", table[i].pseudo, inet_ntoa(table[i].adresse_ip.sin_addr));
    }
}

// envoie en broadcast le message de déconexion '0'
void send_msg_disconnect(){
    int ret;
    char msg_deco[] = "0";
    if((ret = sendto(sid, msg_deco, sizeof(msg_deco), 0, (struct sockaddr*) &broadcast_sock, sizeof(broadcast_sock))) == -1){
        perror("sendto broadcast");
    }
}

void disconnect(int signal){
    send_msg_disconnect();
    if(trace) printf("\nDéconnexion OK.\n");
    exit(0);
}

// initialise le broadcast socket (variable globale)
void init_broadcast_sock(){
    bzero(&broadcast_sock, sizeof(broadcast_sock));
    broadcast_sock.sin_family = AF_INET;
    broadcast_sock.sin_port = htons(PORT);
    broadcast_sock.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    int broadcast_enable = 1;
    if(setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1){
        perror("setsockopt broadcast");
        return 3;
    }
}

/* vérifier les arguments du main */
void check_arguments(int N, char* P[]){
    if(N != 2 && N != 3)
    {
        fprintf(stderr, "Utilisation: %s pseudo -DTRACE.\n", P[0]);
        exit(1);
    }
    if(N == 3 && strcmp(P[2], "-DTRACE")==0){
        trace = 1;
        printf("Mode Trace activé !\n");
    }
}

/* -----*/
/* ----- MAIN ----- */
/* -----*/

/* Parametre : nom -DTRACE
*/
int main(int N, char* P[])
{
    //pour les codes retours
    int ret;
    char buf[LBUF+1];
    struct sockaddr_in client_sock;
    struct sockaddr_in serveur_sock;

    // init du socket pour les broadcasts
    init_broadcast_sock();
    /* vérifie les arguments du main*/
    check_arguments(N,P);

    /* Creation du socket */
    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("socket");
        return(2);
    }

    bzero(&serveur_sock, sizeof(serveur_sock));
    serveur_sock.sin_family = AF_INET;
    serveur_sock.sin_port = htons(PORT);
    serveur_sock.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("Le serveur est attaché au port %d\n", PORT);

    if((ret = bind(sid, (struct sockaddr*)&serveur_sock, sizeof(serveur_sock))) == -1){
        perror("bind");
        return(3);
    }

    struct message msg;
    msg.code = (int)'1';
    strcpy(msg.beuip, "BEUIP");
    strcpy(msg.nom, P[1]);

    buf[0] = msg.code;
    strcpy(&buf[1], msg.beuip);
    strcpy(&buf[6], msg.nom);
    printf("buf pour envoi = %s\n", buf);

    if((ret = sendto(sid, buf, 6+strlen(P[1]), 0, (struct sockaddr*) &broadcast_sock, sizeof(broadcast_sock))) == -1){
        perror("sendto");
    }

    // si ctrl+C : message broadcast 0 puis quitter
    signal(SIGINT, disconnect);

    /* Lecture des messages */
    printf("Ctrl+C pour quitter, début d'écoute.\n");
    do {
        socklen_t sockaddr_taille = sizeof(client_sock);
        if((ret = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr*)&client_sock, &sockaddr_taille)) < 0){
            perror("recvfrom");
            return 4;
        }
        buf[ret] = '\0';
        printf("BRUT: Recu de %s : %s\n", inet_ntoa(client_sock.sin_addr), buf);

        // ack avec nom si broadcast + pas mon message
        if(buf[0] == (int)'1' && strcmp(&buf[6], P[1]) != 0){
            char temp_msg[LBUF];
            sprintf(temp_msg, "2BEUIP%s", P[1]);
            if((ret = sendto(sid, temp_msg, strlen(temp_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sizeof(client_sock))) == -1){
                perror("sendto");
            }
        }

        // ack simple tout le temps si pas mon message ni un ack, ni local
        else if(buf[0] != (int)'2'&& strcmp(&buf[6], P[1]) != 0  && client_sock.sin_addr.s_addr != inet_addr("127.0.0.1")){
            char ack_msg[LBUF];
            sprintf(ack_msg, "2BEUIP%s", P[1]);
            if((ret = sendto(sid, ack_msg, strlen(ack_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sizeof(client_sock))) == -1){
                perror("sendto");
            }
        }

        // message correct d'une autre personne (strNcmp car pas de \0)
        if(strncmp(&buf[1], "BEUIP", 5) == 0){ 
            if(table_wr < TABLE_TAILLE){
                // cherche si adresse déjà connue :
                int connu = 0;
                for(int i=0; i<table_wr; ++i){
                    if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
                        connu = 1;
                        if(trace) printf("Personne connue: %s", table[i].pseudo);
                        break;
                    }
                }
                if(!connu){
                    strcpy(table[table_wr].pseudo, &buf[6]);
                    table[table_wr].adresse_ip = client_sock;
                    if(trace) printf("Nouvel utilisateur : numéro %d, nom: %s, adresse %s\n", table_wr, table[table_wr].pseudo, inet_ntoa(client_sock.sin_addr));
                    ++table_wr;
                }
            }
        }

        // demande d'affichage de la liste : code 3 et demande locale
        if(buf[0] == '3' && (client_sock.sin_addr.s_addr == inet_addr("127.0.0.1"))){
            affiche_liste();
        }

        // demande d'envoi de message privé : code 4 et demande locale
        if(buf[0] == '4' && (client_sock.sin_addr.s_addr == inet_addr("127.0.0.1"))){
            //chercher où commence le message (après premier \0)
            int mess_id = -1;
            for(int i=0; i<LBUF; ++i){
                if(buf[i] == '\0'){
                    mess_id = i+1;
                    break;
                }
                else if(i == LBUF-1) printf("ERREUR recherche message code 4\n");
            }

            // recherche du nom dans la base
            for(int i=0; i<table_wr; ++i){
                if(strcmp(&buf[1], table[i].pseudo) == 0){
                    if(trace) printf("Message transféré à %s : %s", &buf[1], &buf[mess_id]);
                    char sendbuf[LBUF];
                    sendbuf[0] = '9';
                    strcpy(&sendbuf[1], &buf[mess_id]);
                    sendto(sid, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&table[i].adresse_ip, sizeof(table[i].adresse_ip));
                }
            }
        }

        // réception d'un message privé
        if(buf[0] == '9'){
            // trouver le nom de l'expéditeur à partir de l'IP
            for(int i=0; i<table_wr; ++i){
                if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
                    printf("Message de %s : %s", table[i].pseudo, &buf[1]);
                    break;
                }
                else if(i == table_wr-1) printf("ERREUR expéditaire du message privé non trouvé\n");
            }
        }

        // réception d'une demande de message à tous (locale)
        if(buf[0] == '5' && (client_sock.sin_addr.s_addr == inet_addr("127.0.0.1"))){
            if(trace) printf("Envoi du message à tout le monde : %s\n", &buf[1]);
            // envoi à partir de 1 (0 est moi)
            for(int i=1; i<table_wr; ++i){
                sendto(sid, &buf[1], strlen(&buf[1]), 0, (struct sockaddr*)&table[i].adresse_ip, sizeof(table[i].adresse_ip));
            }
        }

        // annonce de déconnexion, message 0
        // supprimer la personne de l'annuaire
        if(buf[0] == '0'){
            for(int i=0; i<table_wr; ++i){
                if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
                    if(trace) printf("%s s'est déconnecté.\n", table[i].pseudo);
                    memset(&table[i].pseudo, '\0', sizeof(table[i].pseudo)); // reset nom
                    memset(&table[i].adresse_ip, 0, sizeof(table[i].adresse_ip)); // reset IP
                }
            }
        }

        printf("\n- - - - - - - - - -\n");
    } while(1);
    
    printf("Fin du programme.\n");
    return 0;
}