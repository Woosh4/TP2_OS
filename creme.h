/* creme.h */
#ifndef CREME_H
#define CREME_H

#include <netinet/in.h>
#include <pthread.h>

extern char VERSION_CREME[];

extern int SERVEUR_LANCE;

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

/* gestion dynamique des interfaces de réseau*/
void diffuser_broadcast_dynamique(int sock_fd, const char* message, int port_dest);
#endif