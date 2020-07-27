#ifndef __NET_H__
#define __NET_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFF_SIZE 131072
#define LISTEN 0
#define SEND   1

typedef struct s_nethandle {
	// Gros addrinfo renvoyé par getaddrinfo contenant ~tout
	struct addrinfo* sainfo;
	// Raccourci vers la première adresse valide sous forme de sockaddr
	struct sockaddr_in6* sin6; // Pointe souvent sur sainfo->ai_addr
	// Ip sous forme de texte
	char* addr; // Pointe parfois sur sainfo->ai_canonname
	int addrlen;
	socklen_t sin6len;
	// Numéro de ma socket
	int socket_desc;
	// Eventuel buffer de données
	void* buf;
	int length;
} nethandle;

int netopen(char* host, char* port, nethandle* s, char c_mode);
int netclose(nethandle* s);
int netlisten(nethandle* s, nethandle* sender);
int netsend_binary(nethandle* s, void* data, int length);
int netsend(nethandle* s, char* str);
// int netmulticast(nethandle* local, nethandle* multi);

#endif
