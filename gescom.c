#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <unistd.h>

#include <string.h>
#include <sys/wait.h>

#include <signal.h>

#include "gescom.h"
#include "creme.h"
#include "servbeuip.h"

/* ------------------------------ définition variables globales*/

char VERSION_GESCOM[] = "1.0";
static char* VERSION_BICEPS = "?"; // vraie version dans biceps.c

ma_chaine prompt_main;
int DTRACE = 0;
char* Mots[MAX_MOTS];
int NMots = 0;
commande_t COMMANDE_INTERNE[NBMAXC];
int commande_interne_count = 0;

/* ------------------------------  fonctions */

/* retourne la position de \0, ou -1 si pas trouvé (> MAX_CHARAC) */
int find_backslash_0(char* chaine){
    for(int i=0; i<MAX_CHARAC; ++i){
        if(chaine[i] == '\0') return i;
    }
    return -1;
}

/* alloue une chaine de taille caractères */
int creer_prompt(ma_chaine* prompt){
    int ret;
    int root;
    prompt->chaine = (char*)malloc(MAX_CHARAC * sizeof(char));
    prompt->cpt = 0;

    char* user = getenv("USER");
    if(strcmp(user, "root") == 0) root=1; else root=0;
    strcpy(prompt->chaine, user);

    prompt->cpt = find_backslash_0(prompt->chaine);

    prompt->chaine[(prompt->cpt)++] = '@';

    ret = gethostname(prompt->chaine+prompt->cpt, MAX_CHARAC-prompt->cpt);
    if(ret != 0){ perror("Erreur creer_prompt gethostname"); exit(1);}

    prompt->cpt = find_backslash_0(prompt->chaine);
    prompt->chaine[(prompt->cpt)++] = root ? '#' : '$';
    prompt->chaine[prompt->cpt] = '\0';
    return ret;
}   

/* désalloue une chaine de caractères */
void destroy_prompt(ma_chaine* prompt){
    free(prompt->chaine);
}

/* alloue un nouveau string, et met le contenu de s dedans (jusqu'au \0) */
char* copyString(char* s){
    return(strdup(s)); // utilise strdup

    // int l = 0;
    // while(s[l] != '\0') ++l;
    // char* string = (char*)malloc((l+1)*sizeof(char));
    // for(int i=0; i<l; ++i){
    //     string[i]=s[i];
    // }
    // string[l]='\0';
    // return string;
}

/* remplis Mots et màj NMots avec b*/
int analyseCom(char* b){
    NMots=0;
    char* rep, *rep2;
    while((rep = strsep(&b, ";")) != NULL){ //découpe par bloc de ;
        while((rep2 = strsep(&rep, " \t\n")) != NULL){ //découpe ces blocs par commandes
            if(rep2[0] != '\0'){
                if(NMots >= (MAX_MOTS-1)){
                    fprintf(stderr, "ERREUR trop d'arguments");
                    break;
                }
                Mots[NMots] = copyString(rep2);
                ++NMots;
            }
        }
        if(b != NULL){
            Mots[NMots] = copyString(";"); // fin bloc ";"
            ++NMots;
        }
    }
    Mots[NMots] = NULL;
    return NMots;
}

/* free les talbeaux de Mots suivant Nmots */
void destroy_Mots(){
    for(int i=0; i<NMots; ++i){
        free(Mots[i]);
        Mots[i] = NULL;
    }
}

void set_version_biceps(char* version_biceps){
    VERSION_BICEPS = version_biceps;
}

/* ------------------------------ commandes internes*/

/* Fais tous les free nécessaires puis quitte le programme */
int Sortie(int N, char *P[]){
    // fermeture automatique du serveur si l'utilisateur ne l'a pas fait
    if (SERVEUR_LANCE) {
        printf("\nFermeture automatique du serveur UDP\n");
        beuip_stop(0, NULL);
    }
    liberer_annuaire();
    destroy_prompt(&prompt_main);
    destroy_Com_Int();
    printf("\nSortie Réussie!\n");
    exit(0);
}

void ajouteCom(char* nom, void (*fonc)){
    if(commande_interne_count >= NBMAXC){fprintf(stderr, "Erreur plus de place dans le tableau de fonctions internes.\n"); exit(1);}
    COMMANDE_INTERNE[commande_interne_count].nom = copyString(nom); // malloc !
    COMMANDE_INTERNE[commande_interne_count].fonc = fonc;
    ++commande_interne_count;
    return;
}

void beuip_comm(int N, char* P[]) {
    if (N < 2) {
        printf("Utilisation : beuip [start|stop|mess] ...\n");
        return;
    }
    
    if (strcmp(P[1], "start") == 0) {
        beuip_start(N - 1, P + 1);
        return;
    }
    if (strcmp(P[1], "stop") == 0) {
        beuip_stop(N - 1, P + 1);
        return;
    }
    if (strcmp(P[1], "list") == 0) {
        beuip_list(N - 1, P + 1);
        return;
    }
    if (strcmp(P[1], "message") == 0) {
        mess(N - 1, P + 1);
        return;
    }
    if (strcmp(P[1], "ls") == 0) {
        if (N < 3) {
            printf("Utilisation : beuip ls pseudo\n");
        } 
        else {
            demandeListe(P[2]);
        }
        return;
    }
    if (strcmp(P[1], "get") == 0) {
        if (N < 4) {
            printf("Utilisation : beuip get pseudo nomfic\n");
        } else {
            demandeFichier(P[2], P[3]);
        }
        return;
    }

    printf("Commande beuip inconnue : '%s'\n", P[1]);
}

void majComInt(void){ /* mise a jour des commandes internes */
    ajouteCom("exit", Sortie);
    ajouteCom("liste", listeComInt);
    ajouteCom("cd", change_directory);
    ajouteCom("pwd", print_work_directory);
    ajouteCom("vers", print_version);
    ajouteCom("history", print_history);
    ajouteCom("beuip", beuip_comm); // commandes beuip gérées dans beuip_comm
}

/* Affiche toutes les commandes internes */
void listeComInt(void){
    printf("Il y a %d commandes internes:\n", commande_interne_count);
    for(int i=0; i<commande_interne_count; ++i){
        printf("%s,\n", COMMANDE_INTERNE[i].nom);
    }
    return;
}

int execComInt(int N, char **P){
    int id = -1;
    for(int i=0; i<commande_interne_count; ++i){
        if(strcmp(P[0], COMMANDE_INTERNE[i].nom) == 0){id=i;break;}
    }
    if(id==-1){
        return 0;
    }
    COMMANDE_INTERNE[id].fonc(N, P);
    return 1;
}

/* Pour free*/
void destroy_Com_Int(void){
    for(int i=0; i<commande_interne_count; ++i) free(COMMANDE_INTERNE[i].nom);
}

void change_directory(int N, char* P[]){
    if(N != 2){
        fprintf(stderr, "Erreur. utilisation: \"cd chemin\"\n");
        return;
    }
    int ret = chdir(P[1]);
    if(ret == -1) perror("chdir");
    return;
}

void print_work_directory(void){
    char tab[MAX_CHARAC];
    tab[MAX_CHARAC-1] = '1';
    getcwd(tab, MAX_CHARAC);
    if(DTRACE && tab[MAX_CHARAC-1] != '1') fprintf(stderr, "pwd: chemin trop grand pour le tableau.\n");
    printf("%s\n", tab);
    return;
}

void print_version(void){
    printf("Biceps version %s; Gescom version %s, Creme version %s, par Alexis Lefèvre ☻\n", VERSION_GESCOM, VERSION_BICEPS, VERSION_CREME);
    return;
}

void print_history(void){
    HIST_ENTRY **the_list = history_list(); //d'après le man
    
    if(the_list != NULL){
        for (int i = 0; the_list[i] != NULL; ++i){
            printf("%d %s\n", i + history_base, the_list[i]->line);
        }
    }
}

/* ------------------------------ commandes externes */

int execComExt(char **P){
    int pipe1[2], pipe2[2], pid, ret;
    char buf[MAX_EXT_BUF];
    if(pipe(pipe1) != 0) {perror("pipe"); return 0;} //Père vers fils
    if(pipe(pipe2) != 0) {perror("pipe"); return 0;} //Fils vers père
    if((pid = fork()) == -1){ perror("fork"); return 0;}

    if(pid == 0){ // Fils
        dup2(pipe1[0], 0); //entrée
        dup2(pipe2[1], 1); // sortie
        execvp(P[0], P);
        perror("Problème Fils");
        exit(1);
    }
    else{ // père
        close(pipe1[1]); // rien à dire
        int status;
        waitpid(pid, &status, 0);
        ret = read(pipe2[0], (void*)buf, MAX_EXT_BUF-1);
        buf[MAX_EXT_BUF-1] = '\0';
        close(pipe2[0]); // plus rien à écouter
        if(ret == -1) perror("Problème read.");
        if(ret >= MAX_EXT_BUF && DTRACE) printf("---DEBUG--- Message reçu du fils mais trop long pour le buffer\n");
        // else ret==0, end of file : OK
        else{
            printf("%s", buf);
            return 1;
        }
    }
    return 0;
}