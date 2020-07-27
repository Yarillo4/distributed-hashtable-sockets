#ifndef __MACROS_H__
#define __MACROS_H__

#define false 0
#define true 1

#define _OK      "\x1B[32m"
#define _INF     "\x1B[36m"
#define _WRN     "\x1B[33m"
#define _ERR     "\x1B[31m"
#define _NORMAL  "\x1B[0m"

#define __info(FILE, ...) {                                        \
	if (get_debug_level() >= 2) {                                  \
		printf("%s[I]%s%s ", _INF, FILE, _NORMAL);                 \
		printf(__VA_ARGS__);                                       \
		printf("\n");                                              \
		fflush(stdout);                                            \
	}                                                              \
}                                                                  \

#define __success(FILE, ...) {                                     \
	if (get_debug_level() >= 2) {                                  \
		printf("%s[S]%s%s ", _OK, FILE, _NORMAL);                  \
		printf(__VA_ARGS__);                                       \
		printf("\n");                                              \
		fflush(stdout);                                            \
	}                                                              \
}                                                                  \

#define __warn(FILE, ...) {                                        \
	if (get_debug_level() >= 1) {                                  \
		if (errno) {                                               \
			fprintf(stderr, "%s[W]%s%s ", _WRN, FILE, _NORMAL);    \
			char buf[1024];                                        \
			sprintf(buf, __VA_ARGS__);                             \
			perror(buf);                                           \
		}                                                          \
		else {                                                     \
			fprintf(stderr, "%s[W]%s%s ", _WRN, FILE, _NORMAL);    \
			fprintf(stderr, __VA_ARGS__);                          \
			fprintf(stderr, "\n");                                 \
		}                                                          \
		fflush(stderr);                                            \
	}                                                              \
}                                                                  \

#define __check(FILE, EXPR, ...){                                  \
	int expr = (EXPR);                                             \
	if ( expr ) {                                                  \
		__success(FILE, __VA_ARGS__);                              \
	}                                                              \
	else {                                                         \
		__warn(FILE, __VA_ARGS__);                                 \
	}                                                              \
}                                                                  \

#define __err(FILE, ...){                                          \
	if (errno) {                                                   \
		fprintf(stderr, "%s[E]%s%s ", _ERR, FILE, _NORMAL);        \
		char buf[1024];                                            \
		sprintf(buf, __VA_ARGS__);                                 \
		perror(buf);                                               \
	}                                                              \
	else {                                                         \
		fprintf(stderr, "%s[E]%s%s ", _ERR, FILE, _NORMAL);        \
		fprintf(stderr, __VA_ARGS__);                              \
		fprintf(stderr, "\n");                                     \
	}                                                              \
	fflush(stderr);                                                \
}                                                                  \

#define __assert(FILE, EXPR, ...){                                 \
	if ( (EXPR) ) {                                                \
		__err(FILE, __VA_ARGS__);                                  \
		exit(EXIT_FAILURE);                                        \
	}                                                              \
}                                                                  \

#define __assert_return(FILE, EXPR, ...){                          \
	if ( (EXPR) ) {                                                \
		__warn(FILE, __VA_ARGS__);                                 \
		return -1;                                                 \
	}                                                              \
}                                                                  \

int get_debug_level(void);

struct __truncated_addrinfo {
//	int ai_flags;
//	int ai_family;
//	int ai_socktype;
//	int ai_protocol;
//
//	socklen_t ai_addrlen;
//	char *ai_canonname;
	struct sockaddr  *ai_addr;
	struct addrinfo  *ai_next;
};

#endif