#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"
#include "config.h"

/**
 * Struct mapping a string to a function pointer
 */
struct f_entry {
	const char name[API_CALL_MAX_LEN];
	void* function;
};

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
void api_handle_request(struct client *cl, char *url);

/**
 * Get a function pointer from the name
 * @name the function name
 * @table the function lookup table, must be in lexical order
 * @table_size the size of the table
 */
void* api_get_function(char* name, const struct f_entry* table, size_t table_size);

#endif
