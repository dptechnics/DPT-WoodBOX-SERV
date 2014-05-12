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

/**
 * Handle api requests
 * @cl the client who sent the request
 * @url the request URL
 */
void api_handle_request(struct client *cl, char *url, struct path_info *pi) {
	printf("Handling api request\r\n");
	cl->response = "{\"data\": \"test\"}";

	/* Write Ok response */
	write_http_header(cl, 200, "OK");
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

	handle_chunk_write(cl);
}
