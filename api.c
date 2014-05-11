#include <sys/types.h>
#include <sys/dir.h>
#include <time.h>
#include <strings.h>
#include <dirent.h>

#include <libubox/blobmsg.h>

#include "uhttpd.h"
#include "mimetypes.h"
#include "api.h"
#include "client.h"

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
bool api_handle_request(struct client *cl, char *url) {


}
