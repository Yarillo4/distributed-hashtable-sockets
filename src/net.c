#include "macros.h"
#include "net.h"

#include <net/if.h>

// Macros d'affichage.
// Je relie chaque macro 'locale' à la macro 'réelle' prenant un argument
// supplémentaire qui s'avère être extrêmement redondant (le nom de fichier...)
#define FILE "        "
#define info(...)          __info(FILE, __VA_ARGS__)
#define success(...)       __success(FILE, __VA_ARGS__)
#define warn(...)          __warn(FILE, __VA_ARGS__)
#define check(...)         __check(FILE, __VA_ARGS__)
#define err(...)           __err(FILE, __VA_ARGS__)
#define assert(...)        __assert(FILE, __VA_ARGS__)
#define assert_return(...) __assert_return(FILE, __VA_ARGS__)

/**
 * @brief Prend un host et un port et appelle getaddrinfo
 * @details 
 * Et je hardcode pour l'ipv6 les datagrammes et l'udp et toutes les options de
 * getaddrinfo.
 * 
 * a b s t r a c t i o n
 * 
 * @param hostname c string host
 * @param port c string port
 * 
 * @return addrinfo
 */
struct addrinfo *resolveHost(char* hostname, char* port){
	int tmp = 0;
	struct addrinfo hints;
	struct addrinfo* result = NULL;

	// J'ai des structures bourrée de flag que je dois initialiser à zéro
	memset(&hints, 0, sizeof(hints));
	// Précisions sur ce que je souhaite comme adresse pour mon hôte
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	if (hostname == NULL){
		hints.ai_flags = AI_PASSIVE;

	}
	else
		hints.ai_flags = AI_CANONNAME;


	// Résolution de l'hôte
	tmp = getaddrinfo(hostname, port, &hints, &result);

	switch (tmp){
		case 0           : 
			return result;
		case EAI_FAMILY  : 
			warn("getaddrinfo: EAI_FAMILY (on [%s]:%s)\n", hostname, port);
			break;
		case EAI_SOCKTYPE: 
			warn("getaddrinfo: EAI_SOCKTYPE (on [%s]:%s)\n", hostname, port);
			break;
		case EAI_BADFLAGS: 
			warn("getaddrinfo: EAI_BADFLAGS (on [%s]:%s)\n", hostname, port);
			break;
		case EAI_NONAME  : 
			warn("getaddrinfo: EAI_NONAME (on [%s]:%s)\n", hostname, port);
			break;
		case EAI_SERVICE : 
			warn("getaddrinfo: EAI_SERVICE (on [%s]:%s)\n", hostname, port);
			break;
		default          :
			warn("getaddrinfo: OTHER (on [%s]:%s)\n", hostname, port);
			break;
	}

	return NULL;
}

/**
 * @brief Convertit un host et un port en un nethandle* que je peux utiliser
 * @details J'utilise le format struct s_nethandle pour écouter et envoyer qui
 * contient beaucoup trop de choses
 * 
 * @param host c string host
 * @param port c string port
 * @param s pointeur sur nethandle de destination
 * @param c_mode 'r' pour écouter, ou 'w' pour envoyer
 * @return 0 ou -1 si erreur
 */
int netopen(char* host, char* port, nethandle* s, char c_mode){
	int mode = SEND;

	// Si je suis en mode SEND, je ne binderai pas ma socket
	if (c_mode == 'r' || c_mode == 'R')
		mode = LISTEN;

	memset(s, 0, sizeof(*s));
	s->sainfo = resolveHost(host, port);
	  assert_return(s->sainfo == NULL, "Bad host?");

	struct addrinfo* p;
	for (p = s->sainfo; p != NULL; p = p->ai_next){
		if ((s->socket_desc = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			warn("socket");
			continue;
		}

		if (mode == LISTEN){
			if (host != NULL){
				if (bind(s->socket_desc, p->ai_addr, p->ai_addrlen) == -1) {
					close(s->socket_desc);
					warn("bind '[%s]:%s'", p->ai_canonname, port);
					continue;
				}
				else {
				  success("Bind : Found a valid address for '[%s]:%s'", host, port);
				}
			}
			else {
				success("Bind : [::]:%s has been passively bound", port);
			}
		}
		else {
			success("Socket opened");
		}

		break;
	}

	assert_return(p == NULL, "%s won't accept IPV6+UDP", host);

	const char* addr;
	if (p->ai_canonname == NULL){
		p->ai_canonname = malloc(28);
		  assert_return(p->ai_canonname == NULL, "malloc");
	}
	
	s->addr = p->ai_canonname;
	s->addrlen = p->ai_addrlen;
	s->sin6 = (struct sockaddr_in6*)s->sainfo->ai_addr;
	
	addr = inet_ntop(
		AF_INET6, 
		// Convertit la sock_addr dans addrinfo en sockaddr_in6* pour récupérer
		// l'adresse ipv6 (récupérée par getaddrinfo)
		&s->sin6->sin6_addr, 
		s->addr, 
		INET6_ADDRSTRLEN
	);
	  assert_return(addr == NULL, "inet_ntop");

	if (s->addr != addr){
		warn("inet_ntop is weird. %p vs. %p", s->addr, addr);
	}

	/*
	 *	Affiche le contenu de l'addrinfo de manière lisible
	 *	En vert (success, debug 2+) si c'est ce que j'attend, 
	 *	en orange (warning, debug 1+) si les valeurs sont inattendues
	 *	
	 *	Exemple (en debug 2+):
	 *	[I] Checking address informations:
	 *	[S]   IP        : V6
	 *	[W]   SOCK_TYPE : Other
	 *	[S]   PROTOCOL  : UDP
	 *	
	 *	Exemple (en debug 1):
	 *	[W]   SOCK_TYPE : Other
	 */
	// Check est une macro de macros.h
	info("Checking address informations (W=Warning, S=Success): ")
		check(p->ai_addr != NULL, 
			"  MEMORY    : %p", p->ai_addr);
		check(s->socket_desc > 0,
			"  SOCKET    : %d", s->socket_desc);
		check(strcmp(s->addr, addr) == 0, 
			"  IP        : %s", s->addr);
		success("  PORT      : %s", port);
		check(p->ai_family == AF_INET6, 
			"  IP TYPE   : V%d", expr?6:4);
		check(p->ai_socktype == SOCK_DGRAM, 
			"  SOCK_TYPE : %s", expr?"Datagram":"Other");
		check(p->ai_protocol == IPPROTO_UDP, 
			"  PROTOCOL  : %s", expr?"UDP":"Other");


	return 0;
}

int netclose(nethandle* s){
	int tmp = 0;
	// Si je suis AI_PASSIVE, je n'ai pas reçu de canonname
	// et j'ai fait un malloc supplémentaire dans s->addr

	if (s->sainfo != NULL){
		if (s->sainfo->ai_canonname == NULL){
			free(s->addr);
		}
		freeaddrinfo(s->sainfo);
	}

	if (s->buf != NULL)
		free(s->buf);


	if (s->socket_desc != 0){
		tmp = close(s->socket_desc);
		assert_return(tmp != 0, "");
	}

	s->sainfo = NULL;
	return true;
}

/*
int addrinfo_to_nethandle(struct addrinfo* ainfo, nethandle* s){
	// todo: gestion d'erreurs
	memset(s, 0, sizeof(nethandle));

	s->sin6 = (struct sockaddr_in6*)ainfo->ai_addr;
	s->addr = malloc(INET6_ADDRSTRLEN);
	  assert_return(s->addr == NULL, "malloc");

	memset(s->addr, 0, INET6_ADDRSTRLEN);

	const char* addr;
	addr = inet_ntop(
		AF_INET6, 
		// Convertit la sock_addr dans addrinfo en sockaddr_in6* pour récupérer
		// l'adresse ipv6 (récupérée par getaddrinfo)
		&s->sin6->sin6_addr, 
		s->addr, 
		INET6_ADDRSTRLEN
	);

	s->sainfo = ainfo;
	s->addrlen = 28;

	return 0;
}
*/

/**
 * @brief Tambouille interne pas intéressante pour vous.
 * @details 
 * Magouille interne convertissant du sockaddr vers mon format nethandle
 * 
 * Utilisé pour transformer les sockaddr renvoyés par 
 * recvfrom() en un nethandle que 'netsend' comprend
 * 
 * Car recvfrom renvoie un sockaddr pour nous permettre de recontacter le noeud
 * nous ayant envoyé un message
 * 
 * @param sockaddr_in6 Sockaddr à convertir
 * @param s Pointeur vers nethandle dans lequel la conversion sera faite
 * 
 * @return 0 ou -1
 */
int sockaddr_to_nethandle(struct sockaddr_in6* sin6, nethandle* s){
	// todo: gestion d'erreurs
	memset(s, 0, sizeof(nethandle));


	assert_return(sin6->sin6_family != AF_INET6, "sockaddr_to_nethandle IPV4");

	s->sin6 = sin6;
	s->addr = malloc(INET6_ADDRSTRLEN);
	  assert_return(s->addr == NULL, "malloc");
	memset(s->addr, 0, INET6_ADDRSTRLEN);

	const char* addr;
	addr = inet_ntop(
		AF_INET6, 
		// Convertit la sock_addr dans addrinfo en sockaddr_in6* pour récupérer
		// l'adresse ipv6 (récupérée par getaddrinfo)
		&s->sin6->sin6_addr, 
		s->addr, 
		INET6_ADDRSTRLEN
	);

	s->sainfo = malloc(sizeof(struct addrinfo));
	  assert_return(s->sainfo == NULL, "malloc");
	*s->sainfo = (struct addrinfo){
		0, 
		AF_INET6, 
		SOCK_DGRAM, 
		IPPROTO_UDP, 
		28, 
		(struct sockaddr*)&s->sin6, 
		NULL, 
		NULL
	};
	s->addrlen = 28;
	s->socket_desc = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	return 0;
}

// /*

/*
 * Enormément de problèmes sur cette fonction
 * Je n'arrive même pas à la faire marcher même en hardcodant tout pour qu'elle 
 * ne fonctionne qu'avec le loopback
 * 
 * rip projet
 */
int netmulticast(nethandle* s, nethandle* multi){
	assert_return(s == NULL, "netjoingroup got null");
	assert_return(s->sin6->sin6_family != AF_INET6, "netjoingroup got ipv4");

	// On fait une requête pour rejoindre un groupe multicast
	int tmp;
	struct ipv6_mreq joinRequest;
	struct addrinfo *resmulti;
	struct addrinfo hints;

	/**
	 * Création socket
	 * 
	 * Problème ici
	 * Est-ce que j'ouvre une nouvelle socket multicast ? Ca ne marche pas
	 * Est-ce que j'utilise l'ancienne socket locale ? Marche pas non plus
	 */
	// socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	multi->socket_desc = s->socket_desc;
	  assert_return(multi->socket_desc == -1, "socket");
	
	/**
	 * Récupération de l'interface et de l'adresse multicast
	 * 
	 * Je fait un resolve sur une ip multicast connue (pour tester)
	 * 
	 * Problème ici
	 * Est-ce que j'envoie mes paquets sur la sockaddr_in6 de ff01::1 ?
	 * Ca ne marche pas dans tous les cas
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	
	// tmp = getaddrinfo("ff01::1", NULL, NULL, &resmulti);
	//   assert_return(tmp == -1, "waw");
	// tmp = addrinfo_to_nethandle(resmulti, multi);
	//   assert_return(tmp == -1, "waw2");
	// multi->sainfo->ai_canonname = "ff01::1";
	// multi->addr = multi->sainfo->ai_canonname;

	/**
	 * Construction de la joinrequest
	 */
	
	// Sans hardcoder:
		// struct sockaddr_in6 *sin6_multi = (struct sockaddr_in6*)&resmulti->ai_addr;
		// struct sockaddr_in6 *sin6_s     = (struct sockaddr_in6*)&s->sainfo->ai_addr;

	// Initialisation de la joinrequest (pas très utile mais j'essaye tout)
	memset(&joinRequest, 0, sizeof(joinRequest));

	// Sans hardcoder: 
		// Même interface que le nethandle en argument
		// Adresse multicast renvoyée par le getaddrinfo plus haut

		// joinRequest.ipv6mr_interface = sin6_s->sin6_scope_id;
		// joinRequest.ipv6mr_multiaddr = sin6_multi->sin6_addr;
	// En hardcodant: 
		joinRequest.ipv6mr_interface = if_nametoindex("lo");
		inet_pton(AF_INET6,"ff01::1",&joinRequest.ipv6mr_multiaddr);

	// Est-ce que je fais le sockopt sur la socket locale ?
	// Est-ce que je le fais sur une nouvelle socket ?
	// Aucun ne marche.
	tmp = setsockopt(multi->socket_desc, IPPROTO_IPV6, IPV6_JOIN_GROUP,
				   &joinRequest, sizeof(joinRequest));
	assert_return(tmp == -1, "setsockopt");

	return 0;
	// ^ Comprendre: 
	// return (rand() % 2 == 0); 
}
//*/

/**
 * @brief Attend un message; renvoie le nb d'octets reçus
 * @details 
 * Message dans s->buf
 * Si sender != NULL, place dans sender les informations pour
 * contacter celui qui a envoyé le message reçu
 * 
 * @param s mon nethandle renvoyé par netopen()
 * @param sender Pointeur sur nethandle ou NULL
 * 
 * @return Nb d'octets reçus ou -1 en cas d'erreur
 */
int netlisten(nethandle* s, nethandle* sender){
	struct sockaddr_storage storage;
	socklen_t storagesize = sizeof(storage);
	memset(&storage, 0, storagesize);


	if (s->buf == NULL){
		s->buf = malloc(BUFF_SIZE);
		  assert_return(s->buf == NULL, "malloc");
		s->length = 0;
	}
	memset(s->buf, 0, BUFF_SIZE);
	
	s->length = recvfrom(
		s->socket_desc, 
		s->buf, 
		BUFF_SIZE, 
		0, // no flags
		(struct sockaddr*)&storage, // pour récupérer d'où vient le message
		&storagesize // taille du storage
	);
	assert_return(s->length == -1, "Recvfrom failed");
	assert_return(s->length ==  0, "Socket %d closed", s->socket_desc);

	if (sender != NULL){
		assert_return(storage.ss_family != AF_INET6, 
					  "netlisten: Received IPV4 sender (%d)", 
					  storage.ss_family);
		if (sockaddr_to_nethandle( (struct sockaddr_in6*)&storage, sender) != 0){
			warn("sockaddr_to_nethandle");
		}
	}

	return s->length;
}


/**
 * @brief Envoie des données binaires
 * @details 
 * 
 * @param s nethandle du destinataire renvoyé par netopen() en mode 'w' 
 * ou par listen
 * @param data buffer de données
 * @param length taille buffer
 * 
 * @return Nb d'octets envoyés ou -1
 */
int netsend_binary(nethandle* s, void* data, int length){
	int tmp;
	
	tmp = sendto(
		s->socket_desc, 
		data, 
		length, 
		0, // no flags
		(struct sockaddr *) s->sin6, 
		sizeof(struct sockaddr_in6)
	);
	if (tmp == -1)
		return -1;

	return s->length;
}

/**
 * @brief Envoie un message
 * @details 
 * 
 * @param s nethandle du destinataire renvoyé par netopen() en mode 'w' 
 * ou par listen
 * @param str null terminated string
 * 
 * @return Nb d'octets envoyés ou -1
 */
int netsend(nethandle* s, char* str){
	int tmp;
	tmp = netsend_binary(s, str, strlen(str));
	  assert_return(tmp == -1, "Sendto %s failed (%s)", s->addr, str);
	return tmp;
}