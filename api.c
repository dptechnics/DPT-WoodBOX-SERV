#include <sys/types.h>
#include <sys/dir.h>
#include <time.h>
#include <strings.h>
#include <dirent.h>

#include <libubox/blobmsg.h>

#include "uhttpd.h"
#include "mimetypes.h"
#include "api.h"

char *response = "{\"app\" : \"DPT WoodBOX\"}";	

static void write_response(struct client *cl)
{
	int r;							/* The number of bytes to copy */
	int copied = 0;					/* The previous number of bytes copied */
	int reslen = strlen(response);	/* The total length of the body */
	int buffsize = sizeof(uh_buf)	/* Size of the response buffer */

	while (cl->us->w.data_bytes < 256) {
		// Determine length to copy from the response buffer
		r = reslen - copied;
		if(buffsize < r) {
			r = buffsize;
		}
		
		/* Request is done when all data is written */
		if (!r) {
			uh_request_done(cl);
			return;
		}
		
		/* Copy string to buffer */
		/* strncpy(uh_buf, response, r); */

		uh_chunk_write(cl, response+copied, r);
		copied = r;
	}
}

static void free_response(struct client *cl)
{
	//TODO
}

bool handle_request(struct client *cl, char *url) {
	printf("handling request: %s", url);
	
	/* write status */
	uh_http_header(cl, 200, "OK");

	ustream_printf(cl->us, "Content-Type: %s\r\n", "application/json");
	ustream_printf(cl->us, "Content-Length: %i\r\n\r\n",strlen(response));

	/* send body */
	if (cl->request.method == UH_HTTP_MSG_HEAD) {
		uh_request_done(cl);
		return true;
	}

	cl->dispatch.write_cb = write_response;
	cl->dispatch.free = free_response;
	cl->dispatch.close_fds = free_response;
	write_response(cl);
	
	return true;
}