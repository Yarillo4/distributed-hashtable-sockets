#include "macros.h"
#include "net.h"

// Macros d'affichage.
// Je relie chaque macro 'locale' à la macro 'réelle' prenant un argument
// supplémentaire qui s'avère être extrêmement redondant (le nom de fichier...)
#define FILE "[CLIENT]"
#define info(...)          __info(FILE, __VA_ARGS__)
#define success(...)       __success(FILE, __VA_ARGS__)
#define warn(...)          __warn(FILE, __VA_ARGS__)
#define check(...)         __check(FILE, __VA_ARGS__)
#define err(...)           __err(FILE, __VA_ARGS__)
#define assert(...)        __assert(FILE, __VA_ARGS__)
#define assert_return(...) __assert_return(FILE, __VA_ARGS__)

enum e_cmd {GET, PUT};

int main(int argc, char **argv) {
	// On commence par checker les arguments
	char *host, *port, *cmd, *hash, *ip;
	enum e_cmd command;

	if (argc == 5 || argc == 6){
		host = argv[1];
		port = argv[2];
		cmd  = argv[3];
		hash = argv[4];
		ip   = argv[5];

		if (argc == 5){
			if (strcmp(cmd, "get") == 0){
				command = GET;
				ip = "";
			} 
			else {
				err("Usage: %s IP PORT get HASH\n", argv[0]);
				exit(EXIT_FAILURE);
			}
		}
		else if (argc == 6){
			if (strcmp(cmd, "put") == 0){
				command = PUT;
			}
			else {
				err("Usage: %s IP PORT put HASH IP\n", argv[0]);
				exit(EXIT_FAILURE);
			}
		}
	} else {
		err("Usage: %s IP PORT COMMANDE HASH [IP]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Les arguments sont valides.
	// Je contacte le serveur
	int tmp;
	nethandle dht;

	// Port valide ?
	tmp = atoi(port);
	  assert(tmp <= 0, "Bad port");

	tmp = netopen(host, port, &dht, 'w');
	  assert(tmp == -1, "Can't connect to DHT");

	// nethandle multi;
	// tmp = netmulticast(&dht, &multi);
	//   	assert(tmp == -1, "Can't join a multicast group");


	char buf[1024];
	sprintf(buf, "%s %s %s", cmd, hash, ip);
	check(
		netsend(&dht, buf) != -1,
		"Send : '%s'", buf
	);

	if (command == GET){
		while (true){
			tmp = netlisten(&dht, NULL);
			info("IP: %s", dht.buf);
			if(strcmp(dht.buf, "(null)") == 0)
				break;
			printf("%s\n", dht.buf);
		}
		fflush(stdout);
	}

	netclose(&dht);

	// print the received char

	// close the socket

	return 0;
}
