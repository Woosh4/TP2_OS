/* creme.h */
#ifndef CREME_H
#define CREME_H

/* pour lancer un fils avec fork pour lancer le serveur avec le pseudo*/
void beuip_start(char* pseudo);

/* demande au serveur de se déconnecter en utilisant ctrl+c sigint*/
void beuip_stop();

/**/
void mess(int argc, char* argv[]);
#endif