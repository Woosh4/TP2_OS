/* creme.c : Commandes Rapides pour l’Envoi de Messages Evolués */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>

/* ----- Variables globales ----- */

char VERSION_CREME[] = "1.0";
int pid_serv;
pid_t PID_SERVEUR = -1; // -1 = serveur OFF
#define LBUF 256

/* ----- fonctions ----- */

void beuip_start(char* pseudo){
    if(PID_SERVEUR != -1) return;
    if((PID_SERVEUR = fork()) == -1){
        perror("fork serveur");
    }
    /* fils : lancer serveur */
    if(PID_SERVEUR == 0){
        execv("./servbeuip", pseudo);
        /* erreur lancement */
        fprintf(stderr, "ERREUR lancement serveur");
        exit(1);
    }
    /* père */
    else{
        return;
    }
}

void beuip_stop(){
    if(PID_SERVEUR == -1) return;
    kill(PID_SERVEUR, SIGINT);
    PID_SERVEUR = -1;
    return;
}

void mess(int argc, char* argv[]){
    if(argc == 0){
        printf("Pas assez d'arguments pour mess");
        return;
    }

    struct sockaddr_in Sock;
    struct hostent *h;
    int pid;

    /* Creation du socket */
    if((pid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
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

    /* liste : arg -l*/
    if(argc == 1 && (strcmp(argv[0], "-l") == 0)){
        if(sendto(pid, "0BEUIP", 7, 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            /*"(struct sockaddr*)&Sock" : cast, puis la fonction en utilisant la famille recast dans le bon type (ipv4, ipv6..)*/
            perror("sendto liste");
            return 4;
        }
        return;
    }

    /* message à une personne : nom msg*/
    if(argc == 2 && (strcmp(argv[0], "all") != 0)){
        char msg_buf[256];
        sprintf(msg_buf, "4BEUIP%s%s%s", '\0', argv[0], argv[1]);
        if(sendto(pid, msg_buf, 6+strlen(msg_buf[6]), 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            /*"(struct sockaddr*)&Sock" : cast, puis la fonction en utilisant la famille recast dans le bon type (ipv4, ipv6..)*/
            perror("sendto msg personne");
            return 4;
        }
        printf("Envoi OK !\n");
        return;
    }

    /* message à tous broadcast : all msg */
    if(argc == 2 && (strcmp(argv[0], "all") == 0)){
        char msg_buf[256];
        sprintf(msg_buf, "5BEUIP%s", argv[1]);
        if(sendto(pid, msg_buf, 6+strlen(msg_buf[6]), 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            /*"(struct sockaddr*)&Sock" : cast, puis la fonction en utilisant la famille recast dans le bon type (ipv4, ipv6..)*/
            perror("sendto msg all");
            return 4;
        }
        printf("Envoi OK !\n");
        return;
    }

    /* si aucun autre cas : erreur */
    fprintf(stderr, "ERREUR mauvaise utilisation de Mess\n");
}