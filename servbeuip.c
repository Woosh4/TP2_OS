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

struct personne{
    char pseudo[30];
    char adresse_ip[20];
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
        printf("%s --- %s", table[i].pseudo, table[i].adresse_ip);
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
    if(N == 3) trace = 1;
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
            if((ret = sendto(sid, "2BEUIPAlexis", 17, MSG_CONFIRM, (struct sockaddr*) &client_sock, sockaddr_taille)) == -1){
                perror("sendto");
            }
        }

        // ack simple tout le temps si pas mon message ni un ack
        if(strcmp(&buf[6], P[1]) != 0 && buf[0] != (int)'2'){
            if((ret = sendto(sid, "2BEUIP Bien reçu de Alexis !", 17, MSG_CONFIRM, (struct sockaddr*) &client_sock, sockaddr_taille)) == -1){
                perror("sendto");
            }
        }

        // message correct d'un autre personne
        if(strcmp(&buf[1], "BEUIP")){ 
            if(table_wr < TABLE_TAILLE){
                // cherche si adresse déjà connu :
                int connu = 0;
                for(int i=0; i<table_wr; ++i){
                    if(strcmp(table[i].adresse_ip, (char*)&client_sock.sin_addr.s_addr) == 0){
                        connu = 1;
                        if(trace) printf("Personne connue: %s", table[i].pseudo);
                        break;
                    }
                }
                if(!connu){
                    memcpy(&table[table_wr].pseudo, &buf[6], strlen(&buf[6]));
                    memcpy(&table[table_wr].adresse_ip, &client_sock.sin_addr.s_addr, sizeof(client_sock.sin_addr.s_addr));
                    if(trace) printf("Nouvel utilisateur : numéro %d, nom: %s, adresse %s\n",table_wr, table[table_wr].pseudo, addrip(ntohl(client_sock.sin_addr.s_addr)));
                    ++table_wr;
                }
            }
        }

        // demande d'affichage de la liste
        // !!!!!!!!!!!!! probablement pas bon
        if(buf[0] == '3' && (client_sock.sin_addr.s_addr == "127.0.0.1")){
            affiche_liste();
        }

        printf("\n- - - - - - - - - -\n");
    } while(1);
    
    printf("Fin du programme.\n");
    return 0;
}