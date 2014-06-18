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
const struct http_response r_ok = { 200, "OK" };
const struct http_response r_bad_req = { 400, "Bad request" };

/**
 * The get handlers table
 */
const struct f_entry get_handlers[3] = {
		{"freespace",  get_free_disk_space },
		{"test", test },
		{"testing", testing}
};

/**
 * The post handlers table
 */
const struct f_entry post_handlers[2] = {
		{"freespace",  get_free_disk_space },
		{"test", test }
};

/**
 * The put handlers table
 */
const struct f_entry put_handlers[2] = {
		{"freespace",  get_free_disk_space },
		{"test", test }
};

/* Lookup table for method handle lookup */
const struct f_entry* handlers[] = {
		get_handlers,
		post_handlers,
		put_handlers
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
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 * @pi info concerning the path
 */
void api_handle_request(struct client *cl, char *url)
{
	json_object *response = NULL; 		/* The response */
	char *request;						/* The request method */

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

	/* Search the correct handler */
	json_object* (*handler)(struct client *) = NULL;

	/* Search get handler */
	handler = api_get_function(request, get_handlers, sizeof(get_handlers)/sizeof(struct f_entry));
	printf("Size: %d\r\n", sizeof(handlers[cl->request.method]));
	printf("Size: %d\r\n", sizeof(get_handlers));

	/* If a handler is found execute it */
	if(handler){
		response = handler(cl);
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
	}else{
		/* Handle bad request */
		cl->http_status = r_bad_req;
		const char* stringResponse = "Request not supported by server.";

		/* Copy the response to the response buffer */
		cl->response = (char*) malloc((strlen(stringResponse)+1)*sizeof(char));
		strcpy(cl->response, stringResponse);
	}

	/* Free request resource if new space was allocated */
	if(firstslash) {
		free(request);
	}

	/* Write the response */
	write_response(cl, cl->http_status.code, cl->http_status.message);
}

/**
 * Get a function pointer from the name
 * @name the function name
 * @table the function lookup table, must be in lexical order
 * @table_size the size of the table
 */
void* api_get_function(char* name, const struct f_entry* table, size_t table_size)
{
	printf("Table size: %d\r\n", table_size);
	fflush(stdout);
	//TODO: optimize to binary search
	for(int i = 0; i < table_size; ++i) {
		printf("Searching index %d/%d, key=%s, stack=%s\r\n", i, 3, name, table[i].name);
		if(strcmp(name, table[i].name) == 0){
			return table[i].function;
		}
	}

	/* Return NULL when no request could be found */
	return NULL;
}
