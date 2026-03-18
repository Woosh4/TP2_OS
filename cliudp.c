/* Exemple de client UDP user datagram protocol 
socket en mode non connecté */

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
    P[1] = nom de la machine serveur
    P[2] = port 
    P[3] = message
*/
int main(int N, char* P[])
{
    int sid;
    struct hostent *h;
    struct sockaddr_in Sock;
    int Sock_len = sizeof(Sock);

    if(N != 4)
    {
        fprintf(stderr, "Utilisation: %s nom_serveur port message\n", P[0]);
        return(1);
    }
    /* Creation du socket */
    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("socket");
        return(2);
    }
    /* Recuperation adresse du serveur */
    if(!(h=gethostbyname(P[1]))){
        perror(P[1]);
        return(3);
    }
    bzero(&Sock, sizeof(Sock));
    Sock.sin_family = AF_INET;
    bcopy(h->h_addr, &Sock.sin_addr, h->h_length);
    Sock.sin_port = htons(atoi(P[2]));
    if(sendto(sid, P[3], strlen(P[3]), 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
        /*"(struct sockaddr*)&Sock" : cast, puis la fonction en utilisant la famille recast dans le bon type (ipv4, ipv6..)*/
        perror("sendto");
        return 4;
    }
    printf("Envoi OK !\n");

    if(recvfrom(sid, BUF, LBUF, 0, (struct sockaddr*)&Sock, (socklen_t*)&Sock_len) == -1){
        perror("recvfrom");
        return 5;
    }
    printf("Reçu en retour : %s\n", BUF);
    return 0;
}