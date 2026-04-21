/* creme.h */
#ifndef CREME_H
#define CREME_H

extern char VERSION_CREME[];

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
#endif