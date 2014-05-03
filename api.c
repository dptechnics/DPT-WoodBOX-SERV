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
	int r, copied = 0;
	int reslen = strlen(response);

	while (cl->us->w.data_bytes < 256) {
		// Determine lenght to copy from the response buffer
		if(sizeof(uh_buf) < reslen - copied) {
			r = sizeof(uh_buf);
		} else {
			r = reslen - copied;
		}
		
		/* Copy string to buffer */
		strncpy(uh_buf, response, r);
		
		/* Check if all data is written */
		if (!r) {
			uh_request_done(cl);
			return;
		}

		uh_chunk_write(cl, uh_buf, r);
		copied = r;
		printf("Writing %i bytes", r);
		printf("Writing %i bytes", r);
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