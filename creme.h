/* creme.h */
#ifndef CREME_H
#define CREME_H

#include <netinet/in.h>
#include <pthread.h>

extern char VERSION_CREME[];

struct personne{
    char pseudo[30];
    struct sockaddr_in adresse_ip;
};

#define TABLE_TAILLE 255
extern struct personne table[TABLE_TAILLE];
extern int table_wr;
extern pthread_mutex_t mutex_annuaire;


/* pour gérer le thread*/
extern void* serveur_udp(void* p);

/* lance un fils avec thread pour lancer le serveur avec le pseudo */
int beuip_start(int argc, char* argv[]);

/* demande au serveur de se déconnecter en utilisant ctrl+c sigint */
int beuip_stop(int argc, char* argv[]);

/* gère les messages : arguments possibles : 
-l pour la liste des personnes connectées
all message pour envoi de message broadcast
nom message pour envoi de message privé
*/
int mess(int argc, char* argv[]);

/* pour gérer les commandes internes */
void commande(char octet1, char * message, char * pseudo);
#endif