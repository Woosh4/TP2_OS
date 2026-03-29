TP2 D'OS USer de Polytech Sorbonne,
Complété entièrement.
Le principe : se connecter au serveur, envoyer le message de connexion "1BEUIP",
puis écouter les réponses "2BEUIP" pour enregistrer les autres personnes dans l'annuaire [nom, ip]
ensuite 2 types de messages : ceux provenant du client local "biceps" pour faire des demandes au serveur de type envoi de message broadcast,
envoi de messages privés par le nom, affichage de l'annuaire
2ème type de message : ceux provenant des autres personnes, qui sont décodés pour savoir le type de message,
et la personne qui les envoie.

le démarrage et l'utilisation du serveur se fait depuis le terminal biceps : il y a 3 commandes;
- beuip_start nom , pour lancer le serveur en arrière plan
- beuip_stop , pour stopper le serveur
- beuip_mess nom message , pour l'envoi d'un message privé à la personne
- beuip_mess all message , pour un message broadcast à tous.

Ces commandes ne fonctionneront pas toutes si le serveur ne peut pas se connecter au boitier.
