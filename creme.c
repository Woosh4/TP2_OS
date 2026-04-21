/* creme.c : Commandes Rapides pour l’Envoi de Messages Evolués */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#include "creme.h"

/* ----- Variables globales ----- */

char VERSION_CREME[] = "1.0";
// int pid_serv;
// pid_t PID_SERVEUR = -1; // -1 = serveur OFF
#define LBUF 256

pthread_t TID_SERVEUR;
int SERVEUR_LANCE = 0;
char pseudo_serveur[30];

/* ----- fonctions ----- */

int beuip_start(int argc, char* argv[]){
    if(argc != 2){
        printf("Utilisation : beuip_start pseudo\n");
        return 1;
    }
    if(SERVEUR_LANCE) {
        printf("Le serveur est déjà lancé \n");
        return 1;
    }

    // copie pseudo
    strncpy(pseudo_serveur, argv[1], 29);
    pseudo_serveur[29] = '\0';

    if(pthread_create(&TID_SERVEUR, NULL, serveur_udp, (void*)pseudo_serveur) != 0){
        perror("pthread_create serveur");
        return 1;
    }

    SERVEUR_LANCE = 1;
    printf("Serveur lancé avec le pseudo '%s'.\n", pseudo_serveur);
    return 0;
}

int beuip_stop(int argc, char* argv[]){
    if(!SERVEUR_LANCE) {
        printf("Erreur: serveur pas encore lancé.\n");
        return 1;
    }

    pthread_cancel(TID_SERVEUR);
    pthread_join(TID_SERVEUR, NULL);
    
    SERVEUR_LANCE = 0;
    printf("Arret du serveur OK.\n");
    return 0;
}

/* pour pouvoir gérer les espaces */
void concatener_message(char* destination, int argc, char* argv[], int debut_idx) {
    destination[0] = '\0';
    for(int i = debut_idx; i < argc; ++i){
        strcat(destination, argv[i]);
        if(i < argc - 1) strcat(destination, " "); // Ajoute un espace entre les mots
    }
}

int mess(int argc, char* argv[]){
    if(argc < 2){
        printf("Utilisation: beuip_mess -l\nbeuip_mess all message\nbeuip_mess nom message\n");
        return 1;
    }

    struct sockaddr_in Sock;
    struct hostent *h;
    int sock_fd;

    if((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }
    if(!(h=gethostbyname("127.0.0.1"))){
        perror("127.0.0.1");
        close(sock_fd);
        return 3;
    }
    bzero(&Sock, sizeof(Sock));
    Sock.sin_family = AF_INET;
    bcopy(h->h_addr, &Sock.sin_addr, h->h_length);
    Sock.sin_port = htons(9998);

    /* arg: -l (liste) */
    if(argc == 2 && (strcmp(argv[1], "-l") == 0)){
        // On envoie simplement "3" pour déclencher l'affichage local du serveur
        if(sendto(sock_fd, "3", 1, 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            perror("sendto liste");
        }
        close(sock_fd);
        return 0;
    }

    /* arg: all message */
    if(argc >= 3 && (strcmp(argv[1], "all") == 0)){
        char msg_buf[256];
        msg_buf[0] = '5';
        concatener_message(&msg_buf[1], argc, argv, 2);

        if(sendto(sock_fd, msg_buf, 1 + strlen(&msg_buf[1]), 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            perror("sendto msg all");
        } else {
            printf("Envoi à tous OK !\n");
        }
        close(sock_fd);
        return 0;
    }

    /* arg: nom message */
    if(argc >= 3 && (strcmp(argv[1], "all") != 0)){
        char msg_buf[256];
        msg_buf[0] = '4';
        
        strcpy(&msg_buf[1], argv[1]); // Copie du pseudo
        int index_msg = 1+1 + strlen(argv[1]); // code+\0 + longueur
        
        concatener_message(&msg_buf[index_msg], argc, argv, 2);
        
        int taille_totale = index_msg + strlen(&msg_buf[index_msg]);

        if(sendto(sock_fd, msg_buf, taille_totale, 0, (struct sockaddr*)&Sock, sizeof(Sock))==-1){
            perror("sendto msg privé");
        } else {
            printf("Envoi privé OK\n");
        }
        close(sock_fd);
        return 0;
    }
    
    // sinon erreur
    fprintf(stderr, "ERREUR mauvaise utilisation de beuip_mess\n");
    close(sock_fd);
    return 1;
}