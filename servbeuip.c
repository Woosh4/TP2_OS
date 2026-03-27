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

// initialise le broadcast socket (variable globale)
void init_broadcast_sock(){
    bzero(&broadcast_sock, sizeof(broadcast_sock));
    broadcast_sock.sin_family = AF_INET;
    broadcast_sock.sin_port = htons(PORT);
    broadcast_sock.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    int broadcast_enable = 1;
    if(setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1){
        perror("setsockopt broadcast");
        exit(3);
    }
}

/* initialise le socket pour le serveur et fais le lien au fd,
en utilisant la variable globale sid */
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

/* Envoie le message de connexion 1BEUIPNom */
void envoi_msg_connexion(const char* mon_pseudo) {
    char buf[LBUF+1];
    sprintf(buf, "1BEUIP");
    strcpy(&buf[6], mon_pseudo);
    if(trace) printf("buf pour envoi = %s\n", buf);

    if(sendto(sid, buf, 6+strlen(mon_pseudo), 0, (struct sockaddr*) &broadcast_sock, sizeof(broadcast_sock)) == -1){
        perror("sendto broadcast connexion");
    }
}

// envoie en broadcast le message de déconexion "0BEUIP"
void send_msg_disconnect(){
    int ret;
    char msg_deco[] = "0BEUIP";
    if((ret = sendto(sid, msg_deco, sizeof(msg_deco), 0, (struct sockaddr*) &broadcast_sock, sizeof(broadcast_sock))) == -1){
        perror("sendto broadcast");
    }
}

void affiche_liste(){
    printf("----- ANNUAIRE -----\n");
    printf("-- PERSONNE -- ADRESSE -- \n");
    for(int i=0; i<table_wr; ++i){
        printf("%s --- %s\n", table[i].pseudo, inet_ntoa(table[i].adresse_ip.sin_addr));
    }
}

/* ajouter une nouvelle personne à l'annuaire, pseudo + ip*/
void ajouter_personne(const char* pseudo, struct sockaddr_in client_sock) {
    if(table_wr >= TABLE_TAILLE) return;
    
    int connu = 0;
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
}

/* retire une personne de l'annuaire en passant par son IP */
void retirer_personne(struct sockaddr_in client_sock) {
    for(int i=0; i<table_wr; ++i){
        if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr){
            if(trace) printf("%s s'est déconnecté.\n", table[i].pseudo);
            memset(&table[i].pseudo, '\0', sizeof(table[i].pseudo)); 
            memset(&table[i].adresse_ip, 0, sizeof(table[i].adresse_ip)); 
            break; // Une seule adresse correspondante
        }
    }
}

/* envoie le message ack 2BEUIP avec mon pseudo ensuite au socket */
void repondre_ack(struct sockaddr_in client_sock, const char* mon_pseudo) {
    char ack_msg[LBUF];
    sprintf(ack_msg, "2BEUIP%s", mon_pseudo);
    if(sendto(sid, ack_msg, strlen(ack_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sizeof(client_sock)) == -1){
        perror("sendto ack");
    }
}

/* gère le code 4 : demande d'envoi de message privé*/
void gerer_envoi_prive_local(const char* buf) {
    int mess_id = -1;
    /* messages du type DESTINATAIRE\0MESSAGE */
    for(int i=0; i<LBUF; ++i){
        if(buf[i] == '\0'){
            mess_id = i+1;
            break;
        }
    }
    if(mess_id == -1) {
        printf("ERREUR recherche message code 4\n");
        return;
    }

    for(int i=0; i<table_wr; ++i){
        if(strcmp(&buf[1], table[i].pseudo) == 0){
            if(trace) printf("Message transféré à %s : %s", &buf[1], &buf[mess_id]);
            char sendbuf[LBUF+7];
            sprintf(sendbuf, "9BEUIP%s", &buf[mess_id]);
            sendto(sid, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&table[i].adresse_ip, sizeof(table[i].adresse_ip));
            return; 
        }
    }
    printf("Utilisateur non trouvé dans l'annuaire.\n");
}

/* code 9 : réception d'un message privé */
void gerer_reception_prive(const char* buf, struct sockaddr_in client_sock) {
    for(int i=0; i<table_wr; ++i){
        if(table[i].adresse_ip.sin_addr.s_addr == client_sock.sin_addr.s_addr && table[i].pseudo[0] != '\0'){
            printf("Message de %s : %s", table[i].pseudo, &buf[6]);
            return;
        }
    }
    printf("ERREUR: expéditeur du message privé non trouvé dans l'annuaire\n");
}

/* code 5 + local : demande d'envoi d'un message à tous, en broadcast */
void diffuser_message_local(const char* buf) {
    if(trace) printf("Envoi du message à tout le monde : %s\n", &buf[1]);
    for(int i=0; i<table_wr; ++i){
        if (table[i].pseudo[0] != '\0') {
            sendto(sid, &buf[1], strlen(&buf[1]), 0, (struct sockaddr*)&table[i].adresse_ip, sizeof(table[i].adresse_ip));
        }
    }
}

/* lance les fonctions suivant le code du message et si il est local ou non */
void traiter_message_recu(char* buf, int ret, struct sockaddr_in client_sock, const char* mon_pseudo) {
    buf[ret] = '\0';
    printf("BRUT: Recu de %s : %s\n", inet_ntoa(client_sock.sin_addr), buf);

    char code = buf[0];
    int is_local = (client_sock.sin_addr.s_addr == inet_addr("127.0.0.1"));

    // ack si nouvelle connexion ou tout autre message qui n'est pas un ack
    // sauf messages locaux et messages reçus de moi même par le broadcast : pas de ack
    if((code == '1' || code != '2') && strcmp(&buf[6], mon_pseudo) != 0 && !is_local){
        repondre_ack(client_sock, mon_pseudo);
    }

    // màj de l'annuaire si le message est correct
    if((strncmp(&buf[1], "BEUIP", 5) == 0) && !is_local){ 
        ajouter_personne(&buf[6], client_sock);
    }

    // choix de l'action suivant le code + local ou pas
    switch(code){
        case '0':
            retirer_personne(client_sock);
            break;
        case '3':
            if(is_local) affiche_liste();
            break;
        case '4':
            if(is_local) gerer_envoi_prive_local(buf);
            break;
        case '5':
            if(is_local) diffuser_message_local(buf);
            break;
        case '9':
            gerer_reception_prive(buf, client_sock);
            break;
    }
    // séparation pour rendre plus lisible
    printf("\n- - - - - - - - - -\n");
}

/* fonction de déconnexion reliée au signal sigint */
void disconnect(int signal){
    send_msg_disconnect();
    if(trace) printf("\nDéconnexion OK.\n");
    exit(0);
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

    /* vérifie les arguments du main*/
    check_arguments(N,P);
    /* Lance le serveur + bind au sid en variable globale */
    init_serveur();
    /* init du socket pour les broadcasts */
    init_broadcast_sock();

    /* Envoi 1BEUIPNom le broadcast de connexion */
    envoi_msg_connexion(P[1]);
    /* si ctrl+C : message broadcast 0 puis quitter */
    signal(SIGINT, disconnect);

    /* Lecture des messages */
    printf("Ctrl+C pour quitter, début d'écoute.\n");
    do {
        socklen_t sockaddr_taille = sizeof(client_sock);
        if((ret = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr*)&client_sock, &sockaddr_taille)) < 0){
            perror("recvfrom");
            return 4;
        }

        traiter_message_recu(buf, ret, client_sock, P[1]);
    } while(1);
    
    printf("Fin du programme.\n");
    return 0;
}