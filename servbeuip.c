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
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "servbeuip.h"

/* ---- vraiables globales ----*/

struct elt * annuaire_head = NULL;
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

void repondre_ack(struct sockaddr_in client_sock, const char* mon_pseudo) {
    char ack_msg[LBUF];
    sprintf(ack_msg, "2BEUIP%s", mon_pseudo);
    if(sendto(sid, ack_msg, strlen(ack_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sizeof(client_sock)) == -1){
        perror("sendto ack");
    }
}

void gerer_reception_prive(const char* buf, struct sockaddr_in client_sock) {
    pthread_mutex_lock(&mutex_annuaire);
    struct elt * curr = annuaire_head;
    while (curr != NULL) {
        if (inet_ntoa(client_sock.sin_addr) == curr->adip){
            printf("Message de %s : %s", curr->nom, &buf[6]);
            pthread_mutex_unlock(&mutex_annuaire);
            return;
        }
        curr = curr->next; // On avance dans la liste
    }   
    pthread_mutex_unlock(&mutex_annuaire);
    printf("ERREUR: expéditeur du message privé non trouvé dans l'annuaire\n");
}

int est_ip_locale(struct sockaddr_in client_sock) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return 0;
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
        
        struct sockaddr_in * addr = (struct sockaddr_in *)ifa->ifa_addr;
        if (addr->sin_addr.s_addr == client_sock.sin_addr.s_addr) {
            freeifaddrs(ifaddr);
            return 1; //locale
        }
    }
    freeifaddrs(ifaddr);
    return 0;
}

void traiter_message_recu(char* buf, int ret, struct sockaddr_in client_sock, const char* mon_pseudo){
    if (est_ip_locale(client_sock)) {
        return; //rien à faire
    }
    buf[ret] = '\0';
    char code = buf[0];

    // ack si nouvelle connexion ou tout autre message qui n'est pas un ack
    // sauf messages locaux et messages reçus de moi même par le broadcast : pas de ack
    if(code == '1' && strcmp(&buf[6], mon_pseudo) != 0){
        repondre_ack(client_sock, mon_pseudo);
    }

    // màj de l'annuaire si le message est correct
    if(strncmp(&buf[1], "BEUIP", 5) == 0){
        ajouteElt(&buf[6], inet_ntoa(client_sock.sin_addr));
    }

    // choix de l'action suivant le code
    switch(code){
        case '0':
            supprimeElt(inet_ntoa(client_sock.sin_addr));
            break;
        case '1':
            // géré par ajouterElt
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
        int old_state;
        //blocage de la fermeture pour pouvoir traiter le message
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);

        traiter_message_recu(buf, ret, client_sock, mon_pseudo);

        //déblocage jusqu'à réception d'un nouveau message
        pthread_setcancelstate(old_state, NULL);
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

void ajouteElt(char * pseudo, char * adip) {
    pthread_mutex_lock(&mutex_annuaire);

    struct elt * curr = annuaire_head;
    struct elt * prev = NULL;

    while (curr != NULL) {
        int cmp = strcmp(curr->nom, pseudo);
        
        if (cmp == 0 || strcmp(curr->adip, adip) == 0) {
            // Pseudo ou IP déjà connu, on ne fait rien
            pthread_mutex_unlock(&mutex_annuaire);
            return;
        }
        
        if (cmp > 0) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    struct elt * nouveau = (struct elt *)malloc(sizeof(struct elt));
    if (nouveau == NULL) {
        perror("malloc ajouteElt");
        pthread_mutex_unlock(&mutex_annuaire);
        return;
    }
    
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0';
    strncpy(nouveau->adip, adip, 15);
    nouveau->adip[15] = '\0';

    nouveau->next = curr;
    if (prev == NULL) {
        annuaire_head = nouveau;
    } else {
        prev->next = nouveau;
    }

    if(trace) printf("Nouvel utilisateur ajouté : %s (%s)\n", nouveau->nom, nouveau->adip);
    pthread_mutex_unlock(&mutex_annuaire);
}

void supprimeElt(char * adip){
    pthread_mutex_lock(&mutex_annuaire);
    
    struct elt * curr = annuaire_head;
    struct elt * prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->adip, adip) == 0) {
            if(trace) printf("%s s'est déconnecté.\n", curr->nom);
            
            if (prev == NULL) {
                annuaire_head = curr->next;
            }
            else {
                prev->next = curr->next;
            }
            
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&mutex_annuaire);
}

void listeElts(void) {
    pthread_mutex_lock(&mutex_annuaire);
    printf("----- ANNUAIRE -----\n");
    printf("-- ADRESSE : PERSONNE -- \n");
    
    struct elt * curr = annuaire_head;
    while (curr != NULL) {
        printf("%s : %s\n", curr->adip, curr->nom);
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&mutex_annuaire);
}

struct elt* trouveEltnom(char* pseudo){
    struct elt * curr = annuaire_head;
    while (curr != NULL) {
        if (strcmp(pseudo, curr->nom) == 0){
            return curr;
        }
        curr = curr->next; // On avance dans la liste
    }
    return NULL;
}

void liberer_annuaire(void) {
    pthread_mutex_lock(&mutex_annuaire); 
    
    struct elt * curr = annuaire_head;
    struct elt * suivant = NULL;

    while (curr != NULL) {
        suivant = curr->next;
        free(curr);          
        curr = suivant;      
    }
    
    annuaire_head = NULL;
    
    pthread_mutex_unlock(&mutex_annuaire);
    if(trace) printf("Annuaire vidé\n");
}

void cleanup_serveur_tcp(void *arg){
    int *listen_fd = (int *)arg;
    close(*listen_fd);
    if(trace) printf("\nArrêt du serveur TCP OK.\n");
}

void* serveur_tcp(void * rep){
    char * directory = (char *)rep;
    int listen_fd, conn_fd;
    struct sockaddr_in servaddr;

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket TCP");
        return NULL;
    }

    // pour empecher "address already in use"
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // adresse 9998
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind TCP");
        close(listen_fd);
        return NULL;
    }
    
    // écoute de 5 max en même temps
    listen(listen_fd, 5); 
    
    if(trace) printf("Serveur TCP démarré sur le port %d, partage le dossier: '%s'\n", PORT, directory);

    // pour fermer le socket listen_fd à la fin du thread + autre éventuellement
    pthread_cleanup_push(cleanup_serveur_tcp, &listen_fd);
    //détruire les zombies
    signal(SIGCHLD, SIG_IGN);
    while(1) {
        conn_fd = accept(listen_fd, (struct sockaddr*)NULL, NULL);
        if (conn_fd < 0) continue;

        if(trace) printf("Nouvelle connexion TCP acceptée !\n");
        


        close(conn_fd); 
    }

    pthread_cleanup_pop(1);
    return NULL;
}

void envoiContenu(int fd, const char * rep) {
    char cmd;
    
    //1er octet
    if (read(fd, &cmd, 1) <= 0) {
        close(fd);
        return;
    }

    if (cmd == 'L') {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork TCP");
            close(fd);
            return;
        }

        if (pid == 0) { // fils
            // Redirection de la sortie standard et d'erreur vers le socket
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);

            // Exécution de ls -l sur le répertoire
            execlp("ls", "ls", "-l", rep, NULL);
            
            perror("Erreur execlp ls");
            exit(1); 
        } 
        else { // père
            //rien à faire: le fils s'en occupe
            close(fd);
        }
    }
    else if (cmd == 'F') { // commande get
        char filename[256];
        int i = 0;
        char c;
        
        while (read(fd, &c, 1) > 0 && c != '\n' && i < 255) {
            filename[i++] = c;
        }
        filename[i] = '\0';

        // pour empecher de remonter l'arborescence
        if (strstr(filename, "../") != NULL || strchr(filename, '/') != NULL) {
            printf("Tentative de piratage bloquée : %s\n", filename);
            close(fd);
            return;
        }

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", rep, filename);

        // verif si fichier distant existe
        if (access(filepath, R_OK) != 0) {
            close(fd);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork TCP cat");
            close(fd);
            return;
        }

        if (pid == 0) { //fils
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            
            execlp("cat", "cat", filepath, NULL);
            perror("Erreur execlp cat");
            exit(1);
        }
        else { //père
            close(fd);
        }
    }
    else{
        // Commande inconnue
        close(fd);
    }
}