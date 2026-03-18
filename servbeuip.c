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

struct personne{
    char pseudo[30];
    struct sockaddr_in adresse_ip;
};

struct message{
    unsigned char code;
    char beuip[6];
    char nom[30];
};

#define LBUF 256
#define PORT 9998
#define TABLE_TAILLE 255
struct personne table[TABLE_TAILLE];
int table_wr = 0;

char* addrip(unsigned long  A){
    static char b[16];
    sprintf(b, "%u.%u.%u.%u",
        (unsigned int)(A>>24&0xFF),
        (unsigned int)(A>>16&0xFF),
        (unsigned int)(A>>8&0xFF),
        (unsigned int)(A&0xFF));
    return b;
}

void affiche_liste(){
    printf("----- ANNUAIRE -----\n");
    printf("-- PERSONNE -- ADRESSE -- \n");
    for(int i=0; i<table_wr; ++i){
        printf("%s --- %s", table[i].pseudo, inet_ntoa(table[i].adresse_ip.sin_addr));
    }
}

/* Parametre : 
*/
int main(int N, char* P[])
{
    int sid;
    int ret;
    char buf[LBUF+1];
    struct sockaddr_in serveur_sock;
    struct sockaddr_in client_sock;
    socklen_t sockaddr_taille;

    struct sockaddr_in broadcast_sock;
    bzero(&broadcast_sock, sizeof(broadcast_sock));
    broadcast_sock.sin_family = AF_INET;
    broadcast_sock.sin_port = htons(PORT);
    broadcast_sock.sin_addr.s_addr = inet_addr("192.168.88.255");
    sockaddr_taille = sizeof(broadcast_sock);
    int trace = 0;

    if(N != 2 && N != 3)
    {
        fprintf(stderr, "Utilisation: %s pseudo -DTRACE.\n", P[0]);
        return(1);
    }
    if(N == 3 && strcmp(P[2], "-DTRACE")==0){
        trace = 1;
        printf("Mode Trace activé !\n");
    }
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

    int broadcastEnable = 1;
    if(setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1){
        perror("setsockopt");
        return 3;
    }

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

    if((ret = sendto(sid, buf, 6+strlen(P[1]), 0, (struct sockaddr*) &broadcast_sock, sockaddr_taille)) == -1){
        perror("sendto");
    }


    /* Lecture des messages */
    printf("Ctrl+C pour quitter, début d'écoute.\n");
    do {
        sockaddr_taille = sizeof(client_sock);
        if((ret = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr*)&client_sock, &sockaddr_taille)) < 0){
            perror("recvfrom");
            return 4;
        }
        buf[ret] = '\0';
        printf("BRUT: Recu de %s : %s\n",addrip(ntohl((client_sock.sin_addr.s_addr))), buf);

        // ack avec nom si broadcast + pas mon message
        if(strcmp(&buf[6], P[1]) != 0 && buf[0] == (int)'1'){
            if((ret = sendto(sid, "2BEUIPAlexis", 14, MSG_CONFIRM, (struct sockaddr*) &client_sock, sockaddr_taille)) == -1){
                perror("sendto");
            }
        }

        // ack simple tout le temps si pas mon message ni un ack
        else if(strcmp(&buf[6], P[1]) != 0 && buf[0] != (int)'2'){
            char ack_msg[LBUF];
            sprintf(ack_msg, "2BEUIP%s", P[1]);
            if((ret = sendto(sid, ack_msg, sizeof(ack_msg), MSG_CONFIRM, (struct sockaddr*) &client_sock, sockaddr_taille)) == -1){
                perror("sendto");
            }
        }

        // message correct d'une autre personne
        if(strcmp(&buf[1], "BEUIP") == 0){ 
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
                    buf[0] = '9';
                    strcpy(&buf[1], &buf[mess_id]);
                    sendto(sid, buf, strlen(buf), 0, (struct sockaddr*)&table[i].adresse_ip, sizeof(table[i].adresse_ip));
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

        printf("\n- - - - - - - - - -\n");
    } while(1);
    
    printf("Fin du programme.\n");
    return 0;
}