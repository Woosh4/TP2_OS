#ifndef SERVBEUIP_H
#define SERVBEUIP_H

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
#include <unistd.h>

#include <ifaddrs.h>
#include <netdb.h>

/* ----- STRUCTURES ----- */

struct message{
    unsigned char code;
    char beuip[6];
    char nom[30];
};

struct personne{
    char pseudo[30];
    struct sockaddr_in adresse_ip;
};

/* -----*/
/* ----- DEFINE ET VARIABLES GLOBALES ----- */
/* -----*/

#define LBUF 256
#define PORT 9998
#define TABLE_TAILLE 255

extern struct personne table[TABLE_TAILLE];
extern int table_wr;
extern pthread_mutex_t mutex_annuaire;

extern int sid;
extern int trace;

/* -----*/
/* ----- FONCTIONS ----- */
/* -----*/

/* initialise le socket pour le serveur et fais le lien au fd,
en utilisant la variable globale sid */
void init_serveur();

/* Envoie le message de connexion 1BEUIPNom */
void envoi_msg_connexion(const char* mon_pseudo);

void affiche_liste();

/* ajouter une nouvelle personne à l'annuaire, pseudo + ip*/
void ajouter_personne(const char* pseudo, struct sockaddr_in client_sock);

/* retire une personne de l'annuaire en passant par son IP */
void retirer_personne(struct sockaddr_in client_sock);

/* envoie le message ack 2BEUIP avec mon pseudo ensuite au socket */
void repondre_ack(struct sockaddr_in client_sock, const char* mon_pseudo);

/* code 9 : réception d'un message privé */
void gerer_reception_prive(const char* buf, struct sockaddr_in client_sock);

/* lance les fonctions suivant le code du message et si il est local ou non */
void traiter_message_recu(char* buf, int ret, struct sockaddr_in client_sock, const char* mon_pseudo);

void cleanup_serveur(void *arg);

void* serveur_udp(void* p);

void diffuser_broadcast_dynamique(int sock_fd, const char* message, int port_dest);

#endif