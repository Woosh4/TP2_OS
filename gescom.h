#ifndef GESCOM_H
#define GESCOM_H

/* ------------------------------ struct */

typedef struct ma_chaine_t{
    char* chaine;
    // combien de char dans la chaine (sans compter le \0)
    int cpt;
    // éléments max : MAX_CHARAC
}ma_chaine;

typedef struct commande_t{
    char* nom;
    int (*fonc)(int argc, char* argv[]);
}commande;

/* ------------------------------  define + global*/

extern char VERSION_GESCOM[];

extern ma_chaine prompt_main;

#define MAX_CHARAC 256 /* max de charactères dans u tableau (prompt, pwd)*/
// #define DEBUG 1 //plus utilisé, DTRACE à la place
extern int DTRACE;

#define MAX_MOTS 10
extern char* Mots[MAX_MOTS]; /* le tableau des mots de la commande */
extern int NMots; /* nombre de mots de la commande */

#define NBMAXC 10 /* Nb maxi de commandes internes */
extern commande COMMANDE_INTERNE[NBMAXC];
extern int commande_interne_count; /* combien de commandes dans le tableau*/

#define MAX_EXT_BUF 512 // taille pour le buffer retour des commandes externes

/* ------------------------------  fonctions */

/* retourne la position de \0, ou -1 si pas trouvé (> MAX_CHARAC) */
int find_backslash_0(char* chaine);

/* alloue une chaine de taille caractères et la remplie avec le prompt*/
int creer_prompt(ma_chaine* prompt);  

/* désalloue une chaine de caractères de type prompt*/
void destroy_prompt(ma_chaine* prompt);

/* alloue un nouveau string, et met le contenu de s dedans (jusqu'au \0) */
char* copyString(char* s);

/* remplis Mots et màj NMots avec b la commande entière, la découpe par espaces et ;*/
int analyseCom(char* b);

/* free les talbeaux de Mots suivant Nmots */
void destroy_Mots();

/* Met à jour la version de biceps dans la var globale statique de gescom.c */
void set_version_biceps(char* version_biceps);

/* ------------------------------ commandes internes*/

/* Fais tous les free nécessaires puis quitte le programme */
int Sortie(int N, char *P[]);

/* ajoute dans COMMANDE_INTERNE le nom et sa fonction associée*/
void ajouteCom(char* nom, void (*fonc));

/* ajoute chaque commande interne*/
void majComInt(void);

/* Affiche toutes les commandes internes */
void listeComInt(void);

/* Cherche puis exécute la commande interne dans P[0]*/
int execComInt(int N, char **P);

/* Pour free COMMANDE_INTERNE */
void destroy_Com_Int(void);

/* équivalent cd */
void change_directory(int N, char* P[]);

/* équivalent pwd */
void print_work_directory(void);

/* affiche la VERSION_GESCOM[] de gescom*/
void print_version(void);

/* affiche l'historique de commandes en utilisant la librairie readline/history*/
void print_history(void);

/* ------------------------------ commandes externes */

/* fork puis lance le fils sur la commande externe P[0]*/
int execComExt(char **P);

#endif