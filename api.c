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
#include "config.h"

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
static void api_handle_request(struct client *cl, char *url, struct path_info *pi) {
	printf("Handling api request\r\n");


}

/**
 * Check if this request should be handled by the api handler
 * @cl the client who send the request
 * @url the request url
 */
static bool api_check_path(struct path_info *pi, const char *url) {
	return uh_path_match(DOCUMENT_ROOT API_PATH, pi->phys);
}

/*
 * The API dispatch handler description
 */
struct dispatch_handler api_dispatch = {
	.script = false,
	.check_path = api_check_path,
	.handle_request = api_handle_request,
};
