#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
void api_handle_request(struct client *cl, char *url, struct path_info *pi);

#endif
