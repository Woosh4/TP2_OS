/* Exemple de client UDP user datagram protocol 
socket en mode non connecté 
Modifié et spécialisé pour le TP2 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define LBUF 256
char BUF[LBUF];

/* Parametre : 
    P[1] = message
    P[2] = message partie 2, les 2 parties sont rassemblées avec \0
*/
int main(int N, char* P[])
{
    int sid;
    struct hostent *h;
    struct sockaddr_in Sock;

    if(N != 2 && N !=3)
    {
        fprintf(stderr, "Utilisation: %s message\n", P[0]);
        return(1);
    }
    /* Creation du socket */
    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("socket");
        return(2);
    }
    /* Recuperation adresse du serveur */
    if(!(h=gethostbyname("127.0.0.1"))){
        perror("127.0.0.1");
        return(3);
    }
    bzero(&Sock, sizeof(Sock));
    Sock.sin_family = AF_INET;
    bcopy(h->h_addr, &Sock.sin_addr, h->h_length);
    Sock.sin_port = htons(atoi("9998"));

    strcpy(BUF, P[1]);
    BUF[strlen(P[1])] = '\0';
    int taille = strlen(BUF);
    if(N==3){
        strcpy(&BUF[strlen(P[1])+1], P[2]);
        taille = strlen(P[1]) + 1 + strlen(P[2]);
    }

    if(sendto(sid, BUF, taille, 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
        /*"(struct sockaddr*)&Sock" : cast, puis la fonction en utilisant la famille recast dans le bon type (ipv4, ipv6..)*/
        perror("sendto");
        return 4;
    }
    printf("Envoi OK !\n");

    // int Sock_len = sizeof(Sock);
    // if(recvfrom(sid, BUF, LBUF, 0, (struct sockaddr*)&Sock, (socklen_t*)&Sock_len) == -1){
    //     perror("recvfrom");
    //     return 5;
    // }
    // printf("Reçu en retour : %s\n", BUF);
    return 0;
}