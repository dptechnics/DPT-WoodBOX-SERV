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
 * Handle response write in chunks
 * cl the client containing the response
 */
static void handle_chunk_write(struct client *cl)
{
	int len;
	int r;
	len = strlen(cl->response);

	while (cl->us->w.data_bytes < 256) {
		r = (len - cl->readidx) > sizeof(uh_buf) ? sizeof(uh_buf) : (len - cl->readidx);
		cl->readidx = r;
		strncpy(uh_buf, cl->response, r);

		if (!r) {
			request_done(cl);
			return;
		}

		uh_chunk_write(cl, uh_buf, r);
	}
}

static void write_response(struct client *cl, int code, char *summary)
{
	/* Write response */
	write_http_header(cl, code, summary);
	ustream_printf(cl->us, "Content-Type: application/json\r\n");
	ustream_printf(cl->us, "Content-Length: %i\r\n\r\n", strlen(cl->response));

	/* Stop if this is a header only request */
	if (cl->request.method == UH_HTTP_MSG_HEAD) {
		request_done(cl);
		return;
	}

	/* Set up client data handlers */
	cl->readidx = 0;
	cl->dispatch.write_cb = handle_chunk_write;		/* Data write handler */
	cl->dispatch.free = NULL;						/* Data free handler */
	cl->dispatch.close_fds = NULL;					/* Data free handler for request */

	/* Start sending data */
	handle_chunk_write(cl);
}

/**
 * Handle GET requests.
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
static void get_request_handler(struct client *cl, char *url)
{
	printf("Handling GET request: %s\r\n", url);
}

/**
 * Handle POST requests.
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
static void post_request_handler(struct client *cl, char *url)
{
	printf("Handling POST request\r\n");

	client_post_data(cl);
}

/**
 * Handle PUT requests.
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
static void put_request_handler(struct client *cl, char *url)
{
	printf("Handling PUT request\r\n");
}


/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
void api_handle_request(struct client *cl, char *url)
{
	/* Check which kind of request it is */
	switch(cl->request.method){
	case UH_HTTP_MSG_GET:
		get_request_handler(cl, url);
		break;
	case UH_HTTP_MSG_POST:
		post_request_handler(cl, url);
		break;
	case UH_HTTP_MSG_PUT:
		put_request_handler(cl, url);
		break;
	default:
		break;
	}

	/* Test reponse */
	cl->response = "{\"data\": \"test\"}";

	/* Write the response */
	write_response(cl, 200, "OK");
}
