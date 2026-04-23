# TP d'OS de Polytech Sorbonne : Partie 3

Le TP a été réalisé jusqu'à la partie 3.3 incluse (transfert de fichiers).

NOM: LEFEVRE
PRENOM: Alexis
---

## Architecture Globale

L'application est divisée en trois couches principales :

1. **L'Interpréteur (BICEPS / `biceps.c` & `gescom.c`) :** - Gère l'interface utilisateur via `readline`.
   - Parse les commandes (internes et externes) et gère l'historique.
   - Dispatche les commandes réseaux vers le module CREME via la fonction `commande_beuip`.

2. **La Logique Applicative (CREME / `creme.c`) :** - Fait le pont entre l'utilisateur et le réseau.
   - Démarre et arrête proprement les threads serveurs.
   - Gère les requêtes client TCP (demande de liste de fichiers, téléchargement).

3. **Le Moteur Réseau (Serveur BEUIP / `servbeuip.c`) :**
   - **Thread UDP (Port 9998) :** Gère la découverte automatique (Broadcast `1BEUIP`), les accusés de réception (`2BEUIP`), et le chat (`9BEUIP`). 
   - **Thread TCP (Port 9998) :** Écoute les requêtes de fichiers. Utilise `fork()` et `dup2()` pour exécuter des commandes système (`ls`, `cat`) et rediriger dynamiquement leur sortie vers le socket réseau de manière asynchrone.
   - **Annuaire Dynamique :** Une liste chaînée protégée par des Mutex (`pthread_mutex_t`) garantissant l'intégrité des données (IP/Pseudos) lors des accès concurrents par les différents threads.

---

## Installation et Utilisation

### Compilation
Un Makefile est fourni à la racine du projet. Pour compiler :
```bash
make go
ou bien
make; ./biceps
```
## Notes :
- La librairie `libreadline-dev` est nécessaire
- Le code a été fait pour fonctionner sous Linux natif, utiliser des alternatives peut provoquer des erreurs (wsl 1/2 par exemple)
- La gestion des interfaces réseau se fait automatiquement, comme demandé dans le sujet; il n'y a plus de #define pour une adresse fixe.
- Utilisation de l'IA: Je me suis aidé de Google Gemini : grandement pour le debug et le readme, et raisonnablement pour la réalisation de certaines fonctions.
