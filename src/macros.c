#include "macros.h"
#include <stdlib.h>

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif

int get_debug_level(void) {
	// If an environment variable has been set
	char *str = getenv("DEBUG_RESEAU");
	
	if (str != NULL)
		return atoi(str);

	// Fallback: if a flag has been set at compile time
	if (DEBUG_LEVEL)
		return (DEBUG_LEVEL);

	return 0;
}