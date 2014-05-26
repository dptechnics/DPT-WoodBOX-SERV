#include <sys/types.h>
#include <sys/dir.h>
#include <time.h>
#include <strings.h>
#include <dirent.h>

#include <libubox/blobmsg.h>
#include <json/json.h>

#include "uhttpd.h"
#include "mimetypes.h"
#include "api.h"
#include "client.h"
#include "config.h"
#include "gethandlers.h"

/* Return code ok */
const struct http_response r_ok = {
		200,
		"OK"
};

/**
 * The get handlers table
 */
const struct f_table get_handlers = {
	1,
	{ "", get_free_disk_space }
};

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
			free(cl->response);
			request_done(cl);
			return;
		}

		uh_chunk_write(cl, uh_buf, r);
	}
}

static void write_response(struct client *cl, int code, const char *summary)
{
	/* Write response */
	write_http_header(cl, code, summary);
	ustream_printf(cl->us, "Content-Type: application/json\r\n");
	ustream_printf(cl->us, "Content-Length: %i\r\n\r\n", strlen(cl->response));

	/* Stop if this is a header only request */
	if (cl->request.method == UH_HTTP_MSG_HEAD) {
		free(cl->response);
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
static json_object * get_request_handler(struct client *cl, char *url, char *request)
{
	json_object * result;	/* The method result */


	/* Store the result */
	result = get_free_disk_space(cl);


	return result;
}

/**
 * Handle POST requests.
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
static json_object * post_request_handler(struct client *cl, char *url, char *request)
{
	return NULL;
}

/**
 * Handle PUT requests.
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
static json_object * put_request_handler(struct client *cl, char *url, char *request)
{
	return NULL;
}


/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
void api_handle_request(struct client *cl, char *url)
{
	json_object *response = NULL; 	/* The response */
	char *request;					/* the GET request method */

	/* Filter the request out of the url */
	char *firstslash = strchr(url + API_STR_LEN, '/');
	if(firstslash){
		/* Get the length of the request when there is a slash after the data */
		size_t len = firstslash - url - API_STR_LEN;
		request = (char*) malloc(len*sizeof(char));
		memcpy(request, url+5, len*sizeof(char));
		request[len] = 0;
	} else {
		/* Request is the same as url without API prefix */
		request = url + API_STR_LEN;
	}

	/* Check which kind of HTTP method it is and delegate*/
	switch(cl->request.method){
	case UH_HTTP_MSG_GET:
		response = get_request_handler(cl, url, request);
		break;
	case UH_HTTP_MSG_POST:
		response = post_request_handler(cl, url, request);
		break;
	case UH_HTTP_MSG_PUT:
		response = put_request_handler(cl, url, request);
		break;
	default:
		break;
	}

	/* Write response when there is one */
	if(response){
		/* Get the string representation of the JSON object */
		const char* stringResponse = json_object_to_json_string(response);

		/* Copy the response to the response buffer */
		cl->response = (char*) malloc((strlen(stringResponse)+1)*sizeof(char));
		strcpy(cl->response, stringResponse);

		/* Free the JSON object */
		json_object_put(response);
	}

	/* Free request resource if new space was allocated */
	if(firstslash) {
		free(request);
	}

	/* Write the response */
	write_response(cl, cl->http_status.code, cl->http_status.message);
}
