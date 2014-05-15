#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"

struct http_response {
	int code;
	const char* message;
};

/* Return code ok */
const struct http_response r_ok = {
		200,
		"OK"
};

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
void api_handle_request(struct client *cl, char *url);

#endif
