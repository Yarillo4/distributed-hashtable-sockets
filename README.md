# Projet-RP
Réseaux et Protocoles - Projet de L3 : Table de hachage distribuée

## Compiler, tester

Notre programme a plusieurs niveaux de debug. Le niveau de debug se définit à la compilation, avec le flag `-DDEBUG_LEVEL=N` (ou `make debug`/`make warnings`/`DEBUG_FLAG=-DDEBUG_LEVEL=N`)
Ce niveau définit la verbosité du programme. Zero pour silencieux, un pour les warnings, deux pour verbeux.

Note: Changer le niveau de debug nécessite un appel à make clean.

Ce niveau de debug peut être écrasé par la variable environnement DEBUG_RESEAU qui prend précédence sur le flag de compilation.

* `make all` Compile client & serveur en mode silencieux
* `make server` Compile seulement le serveur en mode silencieux
* `make client` Compile seulement le client en mode silencieux
* `make debug` Compile en mode debug (999)
* `make warnings` Compile en mode warnings (debug=1)

Le mode debug est ultra stylé, il est donc recommandé de toujours compiler en mode debug.

## Fichier de test

Nous avons inclus un fichier de test `test.sh` dans le rendu pour vérifier que tout fonctionne aussi bien chez vous que chez nous.

Normalement, en mode debug, aucun warning/erreur ne devrait apparaître en exécutant ce fichier (à l'exception du SIGKILL à la fin).

## Questions traitées

### 1.1 Premiers pas
La DHT est en place et le client/serveur sont codés et communiquent entre eux sans problèmes et ce même sur des réseaux distants (modulo les pertes de paquet dans ce cas là).

* GET renvoie toutes les IPs possibles au client lorsqu'il demande un hash
* PUT met le tuple (hash,ip) adjoint à un timestamp dans la DHT

La DHT est un tableau continu. Cf commentaires doxygen de serveur.c 

### 1.2 Connexion et déconnexion entre deux serveurs
Une tentative a été faite pour implémenter le multicast qui aurait permis
aux serveurs de se découvrir entre eux. Malheureusement nous n'avons pas réussi
à implémenter ceci. 
La documentation pour l'IPV6 multicast en C et sur linux est quasi inexistante.

J'ai donc toute l'architecture qui est faite (et qui marche), mais les serveurs
n'arrivent pas à se trouver entre eux.

Vous pouvez tester les commandes `plzgibhashes` et `kktakethis [hash] [ip] [timestamp]` avec `nc -u [host] [port]`

Exemple: 

```
printf "plzgibhashes" | nc -q1 -u 'localhost' '9090'
printf "kktakethis $hash2 client_3 $(date +%s)" | nc -q1 -u 'localhost' '9090'
```

### 1.3 Keep alive entre serveurs
Impossible de le traiter sans le multicast

### 1.4 Obsolescence
La fonction dht_get ne renverra jamais un hash ayant expiré. Le temps par défaut
est de 30 secondes, définissable à la compilation avec 
`-DHASH_DEPRECATION_TIME=`
Un garbage collector a été mis en place pour gérer les hashs périmés. Celui-ci
s'execute toutes les `GARBAGE_COL_TIME` secondes; temps configurable à la
compilation avec `-DGARBAGE_COL_TIME=`

### 1.5 DHT à plus de 2 serveurs
Cette partie fonctionne si le multicast fonctionne également.

### 1.6 Pistes d'amélioration

#### 1.6.1 Rendre le garbage collector moins bloquant

J'aurais pu rendre le GC moins bloquant en empêchant seulement d'accéder aux hash situés après les zones où le GC est déjà passé.

Nous l'implémenterions avec une pthread_cond permettant de vérifier atomiquement
si le garbage collector est passé sur la zone à laquelle on souhaite accéder,
avant d'y accéder.

```C
// Si conflit avec le garbage collector
while (d->gc_hash == i){
	// Attente d'un signal du garbage collector
	pthread_cond_wait(&d->cond, &d->mutex);
}
```

#### 1.6.2 Un thread par commande traitée

Le traitement des commandes (`treat_cmd()`) est extrêmement facile à 
multithreader. Cependant, actuellement tous les accès à la DHT se font en 
section critique, ce qui rendrait un multi threading inutile.

Il aurait fallu une mutex par hash.
* dht_get et dht_put vérifient toujours tous les deux qu'ils passent après le
passage du garbage collector

Arbre d'exécution:

* dht_put crée le hash 
  * Allocation (A), 
  * mutex_init (B), 
  * mutex_lock (C)
  * Remplissage du hash (D)
  * mutex_unlock (E)

Simultanément, dht_get commence par cette opération
```
if (hash != NULL)
   pthread_mutex_lock(mutex);
```

Voici un arbre d'exécution pour prouver que ça ne posera aucun soucis,
A,B,C,D,E les états possibles de dht_put.
* Soit A, B, C
  * Le hash n'existe pas, dht_get renvoie NULL
* Soit D
  * Le hash existe ou n'existe pas. 
    * Si elle existe, la mutex est verrouillée. Je renverrai &IP.
    * Sinon, la mutex existe mais je ne suis pas au courant. Je renvoie NULL.
* Soit E
  * Je renvoie &IP.

#### 1.6.3 Envoyer un ACK pour dht_put

Pour le moment le client ne sait pas si son hash a bien été envoyé ou
non. Il peut éventuellement vérifier lui-même en faisant un GET.

#### 1.6.4 Numéroter les requêtes

J'aurais aimé pouvoir être certain que mes GET et mes PUT arrivent dans l'ordre.
Si j'étais utilisateur de mon programme, j'aurais voulu avoir cette feature.

####  1.6.4 bis Un checksum ?

Il y a peu d'intérêt à implémenter un traitement anti corruption de données mais
la feature pourrait être pratique.

Si la commande se corrompt, ce n'est pas si grave, le serveur ne renvoie rien et le client reessaiera.

Si un PUT se corrompt sur le hash ou l'IP, le hash ne sera jamais demandé, ou 
il pointera sur la mauvaise personne pendant 30s car il ne sera jamais 
réactualisé. Comme GET renvoie toutes les IPs possibles d'un hash le problème
est mineur.

Si un GET se corrompt, le serveur renvoit qu'aucune IP ne possède ce hash pour le moment, et le client retentera plus tard.

Si une réponse au GET se corrompt, le client verra bien que l'IP est mauvaise et il redemandera le hash.

