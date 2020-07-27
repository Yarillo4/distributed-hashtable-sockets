////////////////////////////////////////////////////////////////
//                    Projet de réseau                        //
//   Implémentation naïve du mécanisme général d'une DHT      //
////////////////////////////////////////////////////////////////

#include "macros.h"
#include "net.h"

// cf dht_get, dht_update et garbage_collector
#ifndef HASH_DEPRECATION_TIME
	#define HASH_DEPRECATION_TIME 30
#endif

#ifndef GARBAGE_COL_TIME
	#define GARBAGE_COL_TIME 300
#endif

// Macros d'affichage.
#define FILE "[SERVER]"

// Je relie chaque macro 'locale' à la macro 'réelle' prenant un argument
// supplémentaire qui s'avère être extrêmement redondant (le nom de fichier...)
#define info(...)          __info(FILE, __VA_ARGS__)
#define success(...)       __success(FILE, __VA_ARGS__)
#define warn(...)          __warn(FILE, __VA_ARGS__)
#define check(...)         __check(FILE, __VA_ARGS__)
#define err(...)           __err(FILE, __VA_ARGS__)
#define assert(...)        __assert(FILE, __VA_ARGS__)
#define assert_return(...) __assert_return(FILE, __VA_ARGS__)

#include <pthread.h>
#include <time.h>
#include <signal.h>

typedef struct s_hash {
	char* hash;
	char* ip;
	long int time; // timestamp de la dernière mise à jour
} hash;

/**
 * @brief La structure de la DHT
 * @details 
 * 
 * # Le tableau continu
 * 
 * Ma DHT est un tableau continu de hashs, façon FAT32
 * 
 * J'ai opté pour ne pas utiliser de liste chainée. 
 * Les listes chainées ont des
 * performances abominables chez moi. Chaque hash est à un emplacement 
 * différent dans la mémoire, or j'alloue de gros buffers assez régulièrement p
 * our récupérer et réceptionner des paquets ce qui force les maillons de mes 
 * chaines à être espacés dans la mémoire de plusieurs KO. 
 * Le cache trashing est énorme.
 * 
 * Le compilateur gère également beaucoup mieux les tableaux contigus (voire la
 * différence de performances en -O3)
 * 
 * Le top aurait été un arbre de recherche ou une table de hachage.
 * Mais j'ai préféré mettre en place ce qui touchait au réseau que d'améliorer
 * mes performances.
 * 
 * J'ai également hésité à utiliser les fonctions de la libc mais je me suis dit
 * que je me serais pris zéro.
 * https://www.gnu.org/software/libc/manual/html_node/Hash-Search-Function.html#Hash-Search-Function
 * https://www.gnu.org/software/libc/manual/html_node/Tree-Search-Function.html
 * 
 * # Les mutex et la concurrence
 * 
 * J'ai deux mutex servant a gérer la concurrence entre le garbage collector et
 * les accès à la DHT
 * Le garbage collector et les accès à la DHT sont mutuellement exclusifs.
 * Impossible de stocker une mutex par hash; il faudrait vérifier que la mutex
 * existe avant de la bloquer ce qui n'est pas atomique et rendrait le programme
 * non-déterministe
 */
typedef struct s_dht {
	// Gros tableau dynamique
	hash* htable;
	// indique le dernier hash plein (mis à jour aux appels de dht_add qui
	// rajoutent un hash en fin de table)
	unsigned int cursor;
	// taille allouée pour htable (en unités)
	unsigned int size;
	// premier hash vide 
	// (mis à jour par le garbage collector
	// (et par dht_add lorsqu'il écrase le firstEmpty)
	unsigned int firstEmpty;

	// A verrouiller lorsque la DHT est en train d'être lue/écrite
	pthread_mutex_t mutex;
	// Locked à l'initialisation, unlocked à l'ajout du premier hash. 
	// Le garbage collector a besoin de cette mutex *ouverte* pour se lancer
	pthread_mutex_t gc;
} dht;

/**
 * @brief Libère un tableau de mots renvoyé par string_split
 * @details 
 * 
 * @param words 
 */
void free_split(char** words){
	if (words==NULL)
		return;

	for (int i = 0; words[i] != NULL; ++i)
		free(words[i]);
	free(words);
}

/**
 * @brief Découpe une string en fonction d'un substring entier
 * @details Saute les rendus "strings vides" mais peut être configuré
 * 
 * Ce n'est pas juste trivial; cette fonction gère l'overlap. 
 * Ce n'est pas la même fonctionnalité que strtok ou quelque autre fonction
 * de la libc.
 * 
 * Exemple:
 * 
 * ```C
 * string_split("A  | | B | C |", " | ");
 * ```
 * Renverrait un tableau de cette forme:
 * t[0] = "A "  (pas " A " ou "A")
 * t[1] = "| B"
 * t[2] = "C |" (pas "C" ou segfault)
 * t[3] = (null)
 * 
 * t[3] = NULL pour signifier la fin du tableau.
 * 
 * ```C
 *		char** tab = string_split("A, B, C", ", ");
 *		for (int i = 0; tab[i] != NULL; ++i)
 *			printf("%s\n", tab[i]);
 * ```
 * @param str String à découper
 * @param substring Substring de séparation
 * 
 * @return Un tableau de char* terminé par NULL.
 */
char** string_split(char* str, char* substring){
	char** mots;
	unsigned int iMot = 0;      // Numéro du mot
	//char* iMot = 0x1;          // 
	unsigned int i = 0;			 // Position dans la string
	unsigned int j = 0;			 // Position dans la substring
	unsigned int tailleMot = 0;	 // Taille du mot courant

	mots = malloc(sizeof(char*));

	/*
	 * Nous parcourons le string et comparons chaque caractère
	 * par rapport au début du substring.
	 * Si nous trouvons que str[i] == substring[j], alors nous
	 * incrémentons j
	 * 
	 * Nous ajoutons aussi virtuellement un match à la toute fin
	 * de notre chaîne; car on souhaite aussi découper C dans "A,B,C"
	 * Même si C n'est pas suivi d'une virgule.
	 */

	while (str != NULL) {
		// Si finalement on ne trouve pas notre substring
		if (str[i] != substring[j] && str[i] != '\0') {
			// Nous revenons avant ces cases que nous avons cru être le début
			// de notre substring et reprenons la détection à la case d'après
			i = i - j;
			i++;
			tailleMot += 1;
			j = 0;
		}
		// Sinon, si ce que nous avons en face ressemble à notre substring
		else if (str[i] == substring[j] || str[i] == '\0'){
			j++;
			// Si nous avons un match complet
			if (substring[j] == '\0' || str[i] == '\0'){
				char c = str[i];
				j--;

				// On set remet au début de notre mot
				i -= j; 		// en enlevant la taille de notre substring
				i -= tailleMot; // et de notre mot
				
				/* 
				 * Si nous avons atteint la fin de notre string mais que nous
				 * étions en train de matcher quelque-chose, alors,
				 * ce que nous croyions être un match n'était en fait rien.
				 * Notre mot est donc plus grand que prévu.
				 */
				if (c == '\0')
					tailleMot += j;

				if (tailleMot > 0){
					// On rajoute un pointeur dans le tableau de mots
					mots = realloc(mots, (iMot+2) * sizeof(char*));
					// (le tableau est 1 case trop grande, pour le NULL)

					// Et on alloue de l'espace pour ce mot
					mots[iMot] = malloc(tailleMot);
					// printf("mots[%d] = %p\n", iMot, mots[iMot]);

					// On copie le mot dans notre tableau et on finit par un '\0'
					for (unsigned int n = 0; n < tailleMot; ++n)
						mots[iMot][n] = str[i+n];
					mots[iMot][tailleMot] = '\0';

					// Ca fait un mot de plus.
					iMot++;
				}
				if (c == '\0')
					break;

				// Réinitisalisation de la recherche
				i += j;
				i += tailleMot;
				j = 0;
				tailleMot = 0; 
			}
			i++;
		} 
	}


	// printf("mots[%d] = %p\n", iMot, mots[iMot]);
	mots[iMot] = NULL;
	// mots[0] = (char*)iMot;

	return mots;
}

int dht_init(dht* d){
	int tmp;

	memset(d, 0, sizeof(dht));	  
	tmp = pthread_mutex_init(&d->mutex, NULL);
	  assert_return(tmp == -1, "mutex init");

	tmp = pthread_mutex_init(&d->gc, NULL);
	  assert_return(tmp == -1, "mutex gc init");

	pthread_mutex_lock(&d->gc);

	return 0;
}

/**
 * @brief Libère la mémoire allouée dans la DHT
 * @details 
 * 
 * @param d [description]
 */
void dht_free(dht* d){
	for (unsigned int i = 0; i < d->cursor; ++i){
		if (d->htable[i].hash != NULL){
			free(d->htable[i].hash);
			free(d->htable[i].ip);		
		}
	}
	free(d->htable);
	d->htable = NULL;
}

/**
 * @brief Renvoie l'adresse du hash qui match le tuple (h, ip)
 * @details Pour vérifier si un hash est déjà présent sous une même IP. Ca 
 * change si on fait un dht_add ou un dht_update à l'appel de la commande PUT.
 * 
 * @param d DHT sur laquelle effectuer les opérations
 * @param h string hash
 * @param ip string ip
 * @return ptr hash
 */
hash* dht_getWithIP(dht* d, char* h, char* ip){
	if (d->cursor <= 0)
		return NULL;

	hash* ret = NULL;
	pthread_mutex_lock(&d->mutex);

	for (unsigned int i = 0; i < d->cursor; ++i){
		if (d->htable[i].hash != NULL){
			if ( (strcmp(h, d->htable[i].hash) == 0) &&
				 (strcmp(ip  , d->htable[i].ip)   == 0) )
			{
				ret = &d->htable[i];
				break;
			}
		}
	}
	pthread_mutex_unlock(&d->mutex);
	return ret;
}

/**
 * @brief Renvoie l'adresse du hash dont le hash string est exactement p_search
 * @details 
 * 
 * Ex: 
 * `dht_get(&d, "8962235e792f6b112f04f");` 
 * // Renvoie le premier hash correspondant à '8962235e792f6b112f04f' ou NULL
 * si aucun.
 * `dht_get(&d, NULL);` 
 * // Renvoie le 2e match ou NULL si aucun.
 * `dht_get(&d, NULL);`
 * // Renvoie le 3e match ou NULL si aucun.
 * 
 * @param d DHT sur laquelle effectuer les opérations
 * @param p_search String hash
 * 
 * @return ptr hash ou NULL si pas d'occurrence de hash
 */
hash* dht_get(dht* d, char* p_search){
	if (d->cursor <= 0)
		return NULL;

	static unsigned int i = 0;
	static char* search = NULL;

	if (p_search != NULL){
		i = 0;
		search = p_search;
	}

	hash* ret = NULL;
	pthread_mutex_lock(&d->mutex);

	for (; i < d->cursor; ++i){
		if (d->htable[i].hash != NULL) {
			if (strcmp(d->htable[i].hash, search) == 0){
				ret = &d->htable[i++];
				break;
			}
		}
	}

	pthread_mutex_unlock(&d->mutex);
	return ret;
}

/**
 * @brief Rajoute toujours un hash à la DHT au premier emplacement libre
 * @details Ne vérifie pas si le hash existe déjà dans le tableau.
 * Gère tout seul l'agrandissement du tableau.
 * Met à jour "firstEmpty" au prochain hash null si "firstEmpty" est écrasé
 * Vérifie que le hash ne dépasse pas le HASH_DEPRECATION_TIME avant de le
 * renvoyer
 * 
 * @param d DHT sur laquelle effectuer les opérations
 * @param h Hash
 * @param ip IP
 * @return -1 ou 0
 */
int dht_add(dht* d, char* h, char* ip){
	assert_return(h  == NULL, "Bad command (put - no hash provided)");
	assert_return(ip == NULL, "Bad command (put - no IP provided)");

	pthread_mutex_lock(&d->mutex);

	// On cherche le premier emplacement libre
	int firstHash = (d->htable == NULL);
	hash* s = NULL;
	unsigned int found = 0;
	unsigned int i;
	for (i = d->firstEmpty; i < d->cursor; ++i){
		if (d->htable[i].hash == NULL){
			found = i;
			break;
		}
	}

	// Si on en a trouvé un
	if (found){
		info("Trouvé");
		// On note la position du suivant tant que le tableau est dans le cache
		for (; i < d->cursor; ++i){
			if (d->htable[i].hash == NULL){
				break;
			}
		}
		d->firstEmpty = i;
	}
	else {
		found = d->cursor++;
		d->firstEmpty = d->cursor;
	}

	// Si on manque de place, on agrandit le tableau
	if (d->cursor >= d->size){
		d->size += 512;
		info("  Redim hash table to %d", d->size);
		d->htable = realloc(d->htable, d->size*sizeof(hash));
		  assert_return(d->htable == NULL, "malloc");
	}

	s = &d->htable[found];

	s->hash = malloc(strlen(h));
	  assert_return(s->hash == NULL, "malloc");
	strcpy(s->hash, h);

	s->ip = malloc(strlen(ip));
	  assert_return(s->ip == NULL, "malloc");
	strcpy(s->ip, ip);

	s->time = time(NULL);

	pthread_mutex_unlock(&d->mutex);

	info("  Added hash %s (%s)", s->hash, s->ip);

	if (firstHash){
		// Le premier hash a été ajouté
		// On débloque le garbage collector qui attend p-e depuis le démarrage
		pthread_mutex_unlock(&d->gc);
	}

	return 0;
}

/**
 * @brief Rajoute un tuple dans la DHT ou en met un à jour
 * @details Un hash peut être mis à jour tant qu'il est dans la table.
 * Le garbage collector passe toutes les GARBAGE_COL_TIME secondes
 * 
 * @param d DHT sur laquelle effectuer les opérations
 * @param h Hash
 * @param ip IP
 * @return -1 ou 0
 */
int dht_update(dht* d, char* h, char* ip, char* t){
	assert_return(h == NULL, "Bad command (put - no hash provided)");
	assert_return(ip   == NULL, "Bad command (put - no IP provided)");

	hash* found = dht_getWithIP(d, h, ip);
	if (found == NULL) {
		return dht_add(d, h, ip);
	}
	else {
		if (t == NULL) {
			found->time = time(NULL);
		} else {
			found->time = atol(t);
		}
		info("  Updated hash %s (%s)", found->hash, found->ip);
		return 0;
	}

	return -1;
}

/**
 * @brief Partage un hash
 * @details 
 * Partage un hash à "serv", un ou plusieurs autres serveurs selon si 
 * l'adresse est multicast
 * 
 * @param h c string hash
 * @param serv nethandle*
 * 
 * @return 0 ou -1
 */
int share_hash(hash* h, nethandle* serv){
	(void) serv;
	int tmp;

	char t[21];
	sprintf(t, "%ld", h->time);

	int len = strlen(h->hash) + strlen(h->ip) + strlen(t);

	char str[len+11];

	sprintf(str, "kktakethis %s %s %ld", h->hash, h->ip, h->time);

	info("  Sharing '%s'", str);

	tmp = netsend(serv, str);
	  assert_return(tmp == -1, "share_hash netsend");

	return 0;
}

/**
 * @param d 
 * @param serv Serveur distant
 * 
 * @return 
 */
int share_hashes(dht* d, nethandle* multicast){
	int tmp;

	info("Sharing all of my hashes with %s.", multicast->addr);
	hash* h;
	for (unsigned int i = 0; i < d->cursor; ++i){
		h = &d->htable[i];

		if (h->hash != NULL){
			tmp = share_hash(h, multicast);
			  assert_return(tmp, "share hash fail for %s %s", h->ip, h->hash);
		}
	}

	return 0;
}

/**
 * @brief Traite les commandes reçues par le réseau (via netlisten)
 * @details Execute la commande 'cmd' sur la dht
 * Commandes comprises:
 * - put [str hash] [str ip]
 * - get [str hash]
 * Séparateur d'arguments: espace+
 * 
 * @param d DHT sur laquelle effectuer les opérations
 * @param cmd Commande en question
 * @param sender Certaines commandes ont des valeurs de retour à renvoyer à 
 * celui qui les a envoyé
 * @return -1 ou 0
 */
int treat_cmd(dht* d, char* cmd, nethandle* sender){
	int code = -1;
	char** words = string_split(cmd, " ");
	hash* result;

	// put hash ip
	if (strcmp(words[0], "put") == 0){
		// todo: regarder ce qu'on me donne
		code =  dht_update(d, words[1], words[2], NULL);

		
		////////////////////////////////////////////////////
		//                                                //
		//                Question 1.2                    //
		//                                                //
		//     Le multicast en marche pas, mais           //
		//     en assumant qu'il marche voici comment     //
		//     le programme fonctionnerait                //
		//                                                //
		////////////////////////////////////////////////////
		
		/*
		result = dht_getWithIP(d, words[1], words[2]);
			// Envoie le hash avec share_hash et non put pour éviter les boucles
			// de serveurs qui s'entre-partagent à l'infini un même hash
		code += share_hash(result, groupe_multicast);
		//*/
	}
	// get hash
	else if (strcmp(words[0], "get") == 0) {
		// Look for hashes
		result = dht_get(d, words[1]);
		if (result){
			info("  Found hash %s", result->hash);
		}
		else {
			info("  No hash %s", words[1]);
		}

		// Send every hash
		while (result != NULL) {
			// Si hash encore valide
			if (result->time+HASH_DEPRECATION_TIME >= time(NULL)){
				code = netsend(sender, result->ip);
				if (code == 0){
					info("    Sent ip %s", result->ip);
				} else {
					warn("  netsend failure for %s (%s)", result->ip, result->hash);
				}
			} else {
				info("    Deprecated hash %s", result->ip);
			}
			result = dht_get(d, NULL);
		}
		code = netsend(sender, "(null)");
		info("    Sent (null) terminator");

	}
	// share hashes
	else if (strcmp(words[0], "plzgibhashes") == 0) {
		code = share_hashes(d, sender);
	}
	// receive a hash from another server
	else if (strcmp(words[0], "kktakethis") == 0) {
		info("Received kktakethis from %s", sender->addr);
		// todo: mutex
		if (words[1] && words[2] && words[3]){
			code = dht_update(d, words[1], words[2], words[3]);
		}
	}
	else if (strcmp(words[0], "i_exist") == 0) {
		// todo: keep alive
	}
	else {
		warn("Bad command (unknown): %s ('%s')", cmd, words[0]);
	}

	free_split(words);
	return code;
}

/**
 * @brief [Internal] Quitte le programme en libérant la mémoire
 * @details Appelé lorsque le serveur reçoit sig_term ou sig_int
 * 
 * @param signal
 */
nethandle* _G_PTR_NET_DHT = NULL;
dht*       _G_PTR_DHT     = NULL;

void handle_signal(int signal){
	switch (signal) {
		case SIGINT:
		case SIGTERM:
			if (_G_PTR_NET_DHT)
				netclose(_G_PTR_NET_DHT);
			if (_G_PTR_DHT)
				dht_free(_G_PTR_DHT);
			errno = 0;
			err("Termination signal");
			exit(EXIT_FAILURE);
		default:
			err("Got unknown signal %d", signal);
	}
}

/**
 * @brief Garbage collector en thread séparé
 * @details Libère les hash vieux de GARBAGE_COL_TIME
 * 
 * @param param [description]
 * @return [description]
 */
void* garbage_collector(void* param){
	dht* d = (dht*)param;
	hash* h;
	long int t;
	int size = sizeof(hash);

	pthread_mutex_lock(&d->gc);

	while (d->htable != NULL){
		sleep(HASH_DEPRECATION_TIME);
	
		pthread_mutex_lock(&d->mutex);
		info("Garbage collection started");
		t = time(NULL);

		for (unsigned int i = 0; i < d->cursor; ++i){
			h = &d->htable[i];
			if ( h->hash != NULL && (h->time + GARBAGE_COL_TIME) < t){
				info("  Free of (%s, %s)", h->ip, h->hash);
				free(h->ip);
				free(h->hash);
				memset(h, 0, size);
				if (i < d->firstEmpty){
					d->firstEmpty = i;
				}
			}
		}	
		
		pthread_mutex_unlock(&d->mutex);
		info("Garbage collection done (%lds)", time(NULL)-t);
	}
	
	pthread_mutex_lock(&d->gc);

	return NULL;
}

int main(int argc, char **argv) {	
	int tmp;
	// check the number of args on command line
	if(argc != 3){
		err("Usage: %s IP PORT\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char* host = argv[1];
	char* port = argv[2];

	// Port valide ?
	tmp = atoi(port);
	  assert(tmp <= 0, "Bad port");

	/*
	 * # Gestion des signaux #
	 * 
	 * Avant de faire du malloc en masse on essaye de limiter les dégats
	 * si l'utilisateur tue le serveur
	 */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &handle_signal;
	tmp = sigaction(SIGINT, &sa, NULL);
	  assert(tmp == -1, "Can't handle SIGINT");
	tmp = sigaction(SIGTERM, &sa, NULL);
	  assert(tmp == -1, "Can't handle SIGTERM");


	/*
	 * # Coeur du serveur #
	 * - Ouverture d'une socket d'écoute sur l'ip et le port demandé
	 * - Ecoute sur la socket
	 * - Execution des commandes
	 */
	nethandle s;
	dht my_dht;
	dht_init(&my_dht);

	// Pour garder une trace vers mes pointeurs à libérer en cas de sigterm
	_G_PTR_DHT = &my_dht;
	_G_PTR_NET_DHT = &s;

	info("[W:Warning] [I:Info] [S:Success] [E:Error]");

	// Lancement du nettoyeur de DHT
	pthread_t t_gc; 
	tmp = pthread_create(&t_gc, NULL, &garbage_collector, &my_dht);
	  assert(tmp == -1, "Can't create the garbage collector");

	// Ouverture de la socket
	tmp = netopen(host, port, &s, 'r');
	  assert(tmp == -1, "Can't open host. Bad host/port ?");
	
	//nethandle multi;
	//tmp = netmulticast(&s, &multi);
	//  	if (tmp == -1){
	//  		err("Can't join a multicast group");
	//  	}

	// Ecoute de la socket
	nethandle sender;
	while (true) {
		memset(&sender, 0, sizeof(sender));

		// Todo: multithreading
		tmp = netlisten(&s, &sender);
		if (tmp == -1) {
			warn("Listen failed");
			break;
		}
		
		// Traitement & exécution de la commande
		tmp = treat_cmd(&my_dht, s.buf, &sender);
		// info("Recv : '%s'", s.buf);

		if (tmp == -1){
			warn("Failed: '%s'", s.buf);
		}
		else {
			// success("Treated: '%s'", s.buf);
		}

		netclose(&sender);
	}

	pthread_join(t_gc, NULL);

	info("Leaving !");

	netclose(&s);

	return 0;
}
