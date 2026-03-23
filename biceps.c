/* biceps.c : Bel Interpréteur de Commandes des Elèves de Polytech Sorbonne*/

#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <unistd.h>

#include <string.h>
#include <sys/wait.h>

#include <signal.h>

#include "gescom.h"

/* ------------------------------ version biceps / main */

char VERSION_BICEPS[] = "1.0";

/* ------------------------------ main */

int main(int argc, char* argv[]){
    set_version_biceps(VERSION_BICEPS);
    creer_prompt(&prompt_main);
    if(DTRACE) printf("--DEBUG-- PROMPT=%s\n, cpt=%d\n", prompt_main.chaine, prompt_main.cpt);
    majComInt();
    if(argc > 1 && strcmp(argv[1], "-DTRACE")==0) DTRACE=1;
    signal(SIGINT, SIG_IGN); // ignore ctrl+c

    while(1){
        char* commande = readline(prompt_main.chaine);

        if(commande == NULL){// end of file
            printf("\n");
            Sortie(0, NULL);
            break;
        }
        if (commande[0] == '\0'){ //rien d'entré
            free(commande);
            continue;
        }

        add_history(commande);
        analyseCom(commande);

        int deb = 0;
        while(deb < NMots) {
            int fin = deb;
            
            //cherche la fin de la commande
            while(fin < NMots && strcmp(Mots[fin], ";") != 0) {
                fin++;
            }

            if (fin-deb > 0) {
                char* commande[fin-deb+1];
                for(int i = 0; i < fin-deb; ++i) {
                    commande[i] = Mots[deb + i];
                }
                commande[fin-deb] = NULL;

                //Exécute la sous commande
                if(execComInt(fin-deb, commande) == 0){
                    if(DTRACE) printf("--DEBUG-- Commande non interne\n");
                    if(execComExt(commande) == 0 && DTRACE) {
                        printf("---DEBUG--- Commande non externe\n");
                    }
                }
            }
            deb = fin + 1; //prochain ;
        }
        free(commande);
        destroy_Mots();
    }
    return EXIT_SUCCESS;
}