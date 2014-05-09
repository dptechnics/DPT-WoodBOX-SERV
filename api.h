#ifndef API_H
#define API_H

#include <sys/types.h>

#include "uhttpd.h"
 
bool handle_request(struct client *cl, char *url);
 
#endif