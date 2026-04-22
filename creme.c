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

#include <fcntl.h>

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

pthread_t TID_SERVEUR_TCP;
char REPERTOIRE_PUBLIC[] = "reppub";

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

    //UDP
    if(pthread_create(&TID_SERVEUR, NULL, serveur_udp, (void*)pseudo_serveur) != 0){
        perror("pthread_create serveur");
        return 1;
    }
    //TCP
    if(pthread_create(&TID_SERVEUR_TCP, NULL, serveur_tcp, (void*)REPERTOIRE_PUBLIC) != 0){
        perror("pthread_create TCP");
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
    //demande arret UDP + TCP
    pthread_cancel(TID_SERVEUR);
    pthread_cancel(TID_SERVEUR_TCP);
    //attente arret UDP + TCP
    pthread_join(TID_SERVEUR, NULL);
    pthread_join(TID_SERVEUR_TCP, NULL);
    
    SERVEUR_LANCE = 0;
    printf("Arret des serveurs UDP + TCP OK.\n");
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
    printf("Utilisation invalide de beuip mess: beuip mess [all; nom..]\n");
    return 1;
}

int beuip_list(int argc, char* argv[]){
    if(!SERVEUR_LANCE) {
        printf("Erreur : serveur inactif.\n");
        return 1;
    }
    commande('3', NULL, NULL);
    return 0;
}

void commande(char octet1, char * message, char * pseudo){
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
        }
        else {
            struct sockaddr_in dest;
            dest.sin_family = AF_INET;
            dest.sin_port = htons(9998);
            dest.sin_addr.s_addr = inet_addr(el->adip);
            
            sendto(sid, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&dest, sizeof(dest));
            
        }
        pthread_mutex_unlock(&mutex_annuaire);
    }
    else if (octet1 == '0') { //beuip stop
        char deco_msg[] = "0BEUIP";
        diffuser_broadcast_dynamique(sid, deco_msg, 9998);
    }
    else if (octet1 == '5') { //beuip mess all
        char sendbuf[512];
        sprintf(sendbuf, "9BEUIP%s", message);
        diffuser_broadcast_dynamique(sid, sendbuf, 9998);
    }
}

void demandeListe(char * pseudo) {
    char ip_dest[16];
    
    pthread_mutex_lock(&mutex_annuaire);
    struct elt* el = trouveEltnom(pseudo);
    if (el == NULL) {
        printf("Erreur : l'utilisateur '%s' n'est pas dans l'annuaire.\n", pseudo);
        pthread_mutex_unlock(&mutex_annuaire);
        return;
    }
    strcpy(ip_dest, el->adip);
    pthread_mutex_unlock(&mutex_annuaire);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket TCP client");
        return;
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9998);
    dest.sin_addr.s_addr = inet_addr(ip_dest);

    if (connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("connect TCP");
        close(sockfd);
        return;
    }

    //envoi L
    if (write(sockfd, "L", 1) != 1) {
        perror("write TCP");
        close(sockfd);
        return;
    }

    char buf[512];
    int n;
    printf("--- Fichiers de %s ---\n", pseudo);
    while ((n = read(sockfd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    
    printf("------------------------\n");
    close(sockfd);
}

void demandeFichier(char * pseudo, char * nomfic) {
    char local_path[512];
    snprintf(local_path, sizeof(local_path), "reppub/%s", nomfic);

    // verif si fichier existe localement
    if (access(local_path, F_OK) == 0) {
        printf("Erreur : Le fichier '%s' existe déjà dans le répertoire local.\n", local_path);
        return;
    }

    char ip_dest[16];
    
    pthread_mutex_lock(&mutex_annuaire);
    struct elt* el = trouveEltnom(pseudo);
    if (el == NULL) {
        printf("Erreur : l'utilisateur '%s' n'est pas dans l'annuaire.\n", pseudo);
        pthread_mutex_unlock(&mutex_annuaire);
        return;
    }
    strcpy(ip_dest, el->adip);
    pthread_mutex_unlock(&mutex_annuaire);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket TCP client");
        return;
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9998);
    dest.sin_addr.s_addr = inet_addr(ip_dest);

    if (connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("connect TCP (serveur bien lancé ?)");
        close(sockfd);
        return;
    }

    // envoi de la requête ("Fnomfichier\n")
    char req[512];
    int req_len = snprintf(req, sizeof(req), "F%s\n", nomfic);
    if (write(sockfd, req, req_len) != req_len) {
        perror("write TCP requête");
        close(sockfd);
        return;
    }

    // Ouverture du fichier local pour écriture (Création + Vidage si besoin, permissions 0644)
    int fd_out = open(local_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("Erreur création fichier local");
        close(sockfd);
        return;
    }

    // Lecture du réseau et écriture dans le fichier
    char buf[1024];
    int n;
    int total_bytes = 0;
    
    while ((n = read(sockfd, buf, sizeof(buf))) > 0) {
        if (write(fd_out, buf, n) != n) {
            perror("Erreur écriture fichier local");
            break;
        }
        total_bytes += n;
    }

    close(fd_out);
    close(sockfd);

    // si == 0 : c'est un close(), pas de fichier
    if (total_bytes == 0) {
        printf("Erreur : Le fichier '%s' est introuvable sur le serveur de %s.\n", nomfic, pseudo);
        unlink(local_path); // On supprime le fichier vide local qu'on vient de créer
    } else {
        printf("Téléchargement de '%s' terminé avec succès (%d octets).\n", nomfic, total_bytes);
    }
}