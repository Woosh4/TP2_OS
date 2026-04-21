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
#include <arpa/inet.h>

#include "creme.h"
#include "servbeuip.h"

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
    if(!SERVEUR_LANCE){
        printf("Erreur: serveur pas encore lancé.\n");
        return 1;
    }
    commande('0', NULL, NULL);
    
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
    if(!SERVEUR_LANCE) {
        printf("Erreur : serveur inactif.\n");
        return 1;
    }
    if(argc == 2 && strcmp(argv[1], "-l") == 0){
        commande('3', NULL, NULL);
        return 0;
    }
    if(argc >= 3 && strcmp(argv[1], "all") == 0){
        char msg_buf[256];
        concatener_message(msg_buf, argc, argv, 2);
        commande('5', msg_buf, NULL);
        return 0;
    }
    if(argc >= 3 && strcmp(argv[1], "all") != 0){
        char msg_buf[256];
        concatener_message(msg_buf, argc, argv, 2);
        commande('4', msg_buf, argv[1]);
        return 0;
    }
    printf("Utilisation invalide de beuip mess: beuip mess [-l; all; nom..]\n");
    return 1;
}

void commande(char octet1, char * message, char * pseudo){
    int sock_fd;
    if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket commande");
        return;
    }

    if(octet1 == '3'){ //liste
        listeElts();
    } 
    else if (octet1 == '4'){ // Privé
        char sendbuf[512];
        sprintf(sendbuf, "9BEUIP%s", message);

        pthread_mutex_lock(&mutex_annuaire);
        struct elt* el = trouveEltnom(pseudo);
        
        if (el == NULL) {
            printf("Erreur : l'utilisateur '%s' n'est pas dans l'annuaire.\n", pseudo);
            pthread_mutex_unlock(&mutex_annuaire);
        } else {
            struct sockaddr_in dest;
            dest.sin_family = AF_INET;
            dest.sin_port = htons(9998);
            dest.sin_addr.s_addr = inet_addr(el->adip);
            
            sendto(sock_fd, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&dest, sizeof(dest));
            pthread_mutex_unlock(&mutex_annuaire);
        }
    }
    else if (octet1 == '0') { //beuip stop
        char deco_msg[] = "0BEUIP";
        diffuser_broadcast_dynamique(sock_fd, deco_msg, 9998);
    }
    else if (octet1 == '5') { //beuip mess all
        char sendbuf[512];
        sprintf(sendbuf, "9BEUIP%s", message);
        diffuser_broadcast_dynamique(sock_fd, sendbuf, 9998);
    }

    close(sock_fd);
}