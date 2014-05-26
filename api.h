#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"

/**
 * Struct mapping a string to a function pointer
 */
struct f_entry {
	const char* name;
	void* function;
};

/**
 * Lookup table for functions
 */
struct f_table {
	size_t size;
	struct f_entry* entries;
};

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
void api_handle_request(struct client *cl, char *url);

/**
 * Get a function handler pointer from a string name
 */
void* api_get_function(char* name, const struct f_table table);

#endif
