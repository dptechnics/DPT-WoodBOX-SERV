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

	return false;
}

/**
 * Check if this request should be handled by the api handler
 * @cl the client who send the request
 * @url the request url
 */
bool api_check_path(struct path_info *pi, const char *url) {
	return uh_path_match(conf.cgi_docroot_path, pi->phys);
}

/*
 * The API dispatch handler description
 */
struct dispatch_handler api_dispatch = {
	.script = false,
	.check_path = api_check_path,
	.handle_request = api_handle_request,
};
