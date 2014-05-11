#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
bool api_handle_request(struct client *cl, char *url);

/**
 * Check if this request should be handled by the api handler
 * @cl the client who send the request
 * @url the request url
 */
bool api_check_path(struct path_info *pi, const char *url);

extern struct dispatch_handler api_dispatch;

#endif
