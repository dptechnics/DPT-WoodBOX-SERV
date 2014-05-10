/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: client.c
 * Description: all low level reading and writing occurs here.
 *
 * Created by: Daan Pape
 * Created on: May 10, 2014
 */

#include <libubox/blobmsg.h>
#include <ctype.h>

#include "config.h"
#include "listen.h"
#include "uhttpd.h"
#include "tls.h"
#include "client.h"

/* The list of connected clients */
static LIST_HEAD(clients);

/* The number of connected clients */
int n_clients = 0;

/* Status flag for currently selected client */
static bool client_done = false;

/* The server configuration */
struct config conf = {};

/* Array giving string representation to 'http_version' enum */
const char * const http_versions[] = {
	[UH_HTTP_VER_0_9] = "HTTP/0.9",
	[UH_HTTP_VER_1_0] = "HTTP/1.0",
	[UH_HTTP_VER_1_1] = "HTTP/1.1",
};

/* Array giving string representations to 'http_method' enum */
const char * const http_methods[] = {
	[UH_HTTP_MSG_GET] = "GET",
	[UH_HTTP_MSG_POST] = "POST",
	[UH_HTTP_MSG_HEAD] = "HEAD",
	[UH_HTTP_MSG_PUT] = "PUT",
};

/**
 * Write a http header to a client
 * @client the client to write the header to
 * @code the http status code t o write
 * @summary the http status code info, for example if code = 200, summary = "Ok"
 */
void write_http_header(struct client *cl, int code, const char *summary)
{
	struct http_request *r = &cl->request;
	const char *enc = "Transfer-Encoding: chunked\r\n";
	const char *conn;

	/* If no chunked transfer is used, remove the encoding line */
	if (!uh_use_chunked(cl))
		enc = "";

	/* Check if connection should be closed or kept open after request */
	if (r->connection_close)
		conn = "Connection: close";
	else
		conn = "Connection: Keep-Alive";

	/* Send the header to the client */
	ustream_printf(cl->us, "%s %03i %s\r\n%s\r\n%s",
		http_versions[cl->request.version],
		code, summary, conn, enc);

	/* If this is a Keep-Alive connection, send the keep alive time */
	if (!r->connection_close)
		ustream_printf(cl->us, "Keep-Alive: timeout=%d\r\n", KEEP_ALIVE_TIME);
}

/**
 * Close this client connection
 * @client the client to close the connection from
 */
static void close_connection(struct client *cl)
{
	cl->state = CLIENT_STATE_CLOSE;
	cl->us->eof = true;
	ustream_state_change(cl->us);
}

/**
 * Free the dispatch method resources
 * @client the client to free the resources from
 */
static void dispatch_done(struct client *cl)
{
	if (cl->dispatch.free)
		cl->dispatch.free(cl);
	if (cl->dispatch.req_free)
		cl->dispatch.req_free(cl);
}

/**
 * This function is called when a client times out. The connection
 * should then be closed.
 * @timeout: the timeout event from the uloop event loop
 */
static void timeout_event_handler(struct uloop_timeout *timeout)
{
	/* Get the client that caused the timeout event */
	struct client *cl = container_of(timeout, struct client, timeout);

	/* Close the connection */
	close_connection(cl);
}

/**
 * This handler should be installed on the event loop timeout event
 * when a keepalive connections is used. It wil keep the connection
 * open when needed and close when real timeout has happened.
 * @timeout: the timeout event from the uloop event loop
 */
static void polling_event_handler(struct uloop_timeout *timeout)
{
	/* Get the client that caused the timeout event */
	struct client *cl = container_of(timeout, struct client, timeout);

	/* Set timeout when the client had made request, network timeout otherwise */
	int msec = cl->requests > 0 ? KEEP_ALIVE_TIME : NETWORK_TIMEOUT;

	/* Install closing event handler on the connection */
	cl->timeout.cb = timeout_event_handler;
	uloop_timeout_set(&cl->timeout, msec * 1000);
	cl->us->notify_read(cl->us, 0);
}

/**
 * Poll the connection to keep it alive
 */
static void poll_connection(struct client *cl)
{
	/* Install polling event handler on the connection */
	cl->timeout.cb = polling_event_handler;
	uloop_timeout_set(&cl->timeout, 1);
}

/**
 * Signal a request is done and set the connection to wait
 * for another request from the client.
 * @cl the client from which the request is done
 */
void request_done(struct client *cl)
{
	/* Send EOF to client and free dispatch resources */
	uh_chunk_eof(cl);
	dispatch_done(cl);

	/* Set the dispatch pointers to zero */
	memset(&cl->dispatch, 0, sizeof(cl->dispatch));

	/* If this is no Keep-Alive connection close it */
	if (!KEEP_ALIVE_TIME || cl->request.connection_close){
		close_connection(cl);
	} else {
		/* Else wait for new requests and poll the connection to keep it alive */
		cl->state = CLIENT_STATE_INIT;
		cl->requests++;
		poll_connection(cl);
	}
}

/**
 * Send an error message to the browser
 * @cl the client to send the error message to
 * @code the error code to write to the client
 * @summary the code description, for example code = 500, summary = "Internal Server Error"
 * @fmt optional error information
 */
void __printf(4, 5) send_client_error(struct client *cl, int code, const char *summary, const char *fmt, ...)
{
	va_list arg;

	/* Write the header with the error code */
	write_http_header(cl, code, summary);

	/* Set the content type to html */
	ustream_printf(cl->us, "Content-Type: text/html\r\n\r\n");

	/* Send the code summary in heading */
	uh_chunk_printf(cl, "<h1>%s</h1>", summary);

	/* If there is optional info send it */
	if (fmt) {
		va_start(arg, fmt);
		uh_chunk_vprintf(cl, fmt, arg);
		va_end(arg);
	}

	/* End the request */
	request_done(cl);
}

/**
 * Handle header errors.
 * @cl the client that send wrong headers
 * @code the error code
 * @summary the code description
 */
static void header_error(struct client *cl, int code, const char *summary)
{
	send_client_error(cl, code, summary, NULL);
	close_connection(cl);
}

/**
 * This helper method is used to find the index of the http_methods
 * and http_version enums from the header string.
 * @list the list of possible strings
 * @max the length of the list to search in
 * @str the string to search in
 */
static int find_idx(const char * const *list, int max, const char *str)
{
	int i;

	for (i = 0; i < max; i++)
		if (!strcmp(list[i], str))
			return i;

	return -1;
}

/**
 * Parse an incoming client request. This is installed as a handler
 * @cl: the client that sent the request
 * @data: the request data
 */
static int parse_client_request(struct client *cl, char *data)
{
	struct http_request *req = &cl->request;
	char *type, *path, *version;
	int h_method, h_version;

	/* Split the the data on spaces */
	type = strtok(data, " ");
	path = strtok(NULL, " ");
	version = strtok(NULL, " ");
	if (!type || !path || !version)
		return CLIENT_STATE_DONE;

	/* Add the path to the client header */
	blobmsg_add_string(&cl->hdr, "URL", path);

	/* Clear the previous request info from the client */
	memset(&cl->request, 0, sizeof(cl->request));

	/* Find the enums corresponding to the method and type */
	h_method = find_idx(http_methods, ARRAY_SIZE(http_methods), type);
	h_version = find_idx(http_versions, ARRAY_SIZE(http_versions), version);

	/* Check if the method is supported */
	if (h_method < 0 || h_version < 0) {
		req->version = UH_HTTP_VER_1_0;
		return CLIENT_STATE_DONE;
	}

	/* Save the http method en version */
	req->method = h_method;
	req->version = h_version;

	/* Close connection when needed */
	if (req->version < UH_HTTP_VER_1_1 || req->method == UH_HTTP_MSG_POST || !conf.http_keepalive)
		req->connection_close = true;

	/* Set the state as header parsed */
	return CLIENT_STATE_HEADER;
}

/**
 * This function is called when a client makes a new request
 */
static bool client_init_handler(struct client *cl, char *buf, int len)
{
	char *newline;

	printf("%s", buf);

	/* Get the first newline in the the header, if there is no newlien
	 * the header is faulty */
	newline = strstr(buf, "\r\n");
	if (!newline)
		return false;

	/* If the buffer only contains a newline, consume them */
	if (newline == buf) {
		ustream_consume(cl->us, 2);
		return true;
	}

	/* Parse the header in the buffer and consume the stream */
	*newline = 0;
	blob_buf_init(&cl->hdr, 0);
	cl->state = parse_client_request(cl, buf);
	ustream_consume(cl->us, newline + 2 - buf);

	/* Return an error when the header is malformed */
	if (cl->state == CLIENT_STATE_DONE)
		header_error(cl, 400, "Bad Request");

	return true;
}

static bool rfc1918_filter_check(struct client *cl)
{
	if (!conf.rfc1918_filter)
		return true;

	if (!uh_addr_rfc1918(&cl->peer_addr) || uh_addr_rfc1918(&cl->srv_addr))
		return true;

	send_client_error(cl, 403, "Forbidden",
			"Rejected request from RFC1918 IP "
			"to public server address");
	return false;
}

static void client_header_complete(struct client *cl)
{
	struct http_request *r = &cl->request;

	if (!rfc1918_filter_check(cl))
		return;

	if (r->expect_cont)
		ustream_printf(cl->us, "HTTP/1.1 100 Continue\r\n\r\n");

	switch(r->ua) {
	case UH_UA_MSIE_OLD:
		if (r->method != UH_HTTP_MSG_POST)
			break;

		/* fall through */
	case UH_UA_SAFARI:
		r->connection_close = true;
		break;
	default:
		break;
	}

	uh_handle_request(cl);
}

static void client_parse_header(struct client *cl, char *data)
{
	struct http_request *r = &cl->request;
	char *err;
	char *name;
	char *val;

	if (!*data) {
		uloop_timeout_cancel(&cl->timeout);
		cl->state = CLIENT_STATE_DATA;
		client_header_complete(cl);
		return;
	}

	val = uh_split_header(data);
	if (!val) {
		cl->state = CLIENT_STATE_DONE;
		return;
	}

	for (name = data; *name; name++)
		if (isupper(*name))
			*name = tolower(*name);

	if (!strcmp(data, "expect")) {
		if (!strcasecmp(val, "100-continue"))
			r->expect_cont = true;
		else {
			header_error(cl, 412, "Precondition Failed");
			return;
		}
	} else if (!strcmp(data, "content-length")) {
		r->content_length = strtoul(val, &err, 0);
		if (err && *err) {
			header_error(cl, 400, "Bad Request");
			return;
		}
	} else if (!strcmp(data, "transfer-encoding")) {
		if (!strcmp(val, "chunked"))
			r->transfer_chunked = true;
	} else if (!strcmp(data, "connection")) {
		if (!strcasecmp(val, "close"))
			r->connection_close = true;
	} else if (!strcmp(data, "user-agent")) {
		char *str;

		if (strstr(val, "Opera"))
			r->ua = UH_UA_OPERA;
		else if ((str = strstr(val, "MSIE ")) != NULL) {
			r->ua = UH_UA_MSIE_NEW;
			if (str[5] && str[6] == '.') {
				switch (str[5]) {
				case '6':
					if (strstr(str, "SV1"))
						break;
					/* fall through */
				case '5':
				case '4':
					r->ua = UH_UA_MSIE_OLD;
					break;
				}
			}
		}
		else if (strstr(val, "Chrome/"))
			r->ua = UH_UA_CHROME;
		else if (strstr(val, "Safari/") && strstr(val, "Mac OS X"))
			r->ua = UH_UA_SAFARI;
		else if (strstr(val, "Gecko/"))
			r->ua = UH_UA_GECKO;
		else if (strstr(val, "Konqueror"))
			r->ua = UH_UA_KONQUEROR;
	}


	blobmsg_add_string(&cl->hdr, data, val);

	cl->state = CLIENT_STATE_HEADER;
}

void client_poll_post_data(struct client *cl)
{
	struct dispatch *d = &cl->dispatch;
	struct http_request *r = &cl->request;
	char *buf;
	int len;

	if (cl->state == CLIENT_STATE_DONE)
		return;

	while (1) {
		char *sep;
		int offset = 0;
		int cur_len;

		buf = ustream_get_read_buf(cl->us, &len);
		if (!buf || !len)
			break;

		if (!d->data_send)
			return;

		cur_len = min(r->content_length, len);
		if (cur_len) {
			if (d->data_blocked)
				break;

			if (d->data_send)
				cur_len = d->data_send(cl, buf, cur_len);

			r->content_length -= cur_len;
			ustream_consume(cl->us, cur_len);
			continue;
		}

		if (!r->transfer_chunked)
			break;

		if (r->transfer_chunked > 1)
			offset = 2;

		sep = strstr(buf + offset, "\r\n");
		if (!sep)
			break;

		*sep = 0;

		r->content_length = strtoul(buf + offset, &sep, 16);
		r->transfer_chunked++;
		ustream_consume(cl->us, sep + 2 - buf);

		/* invalid chunk length */
		if (sep && *sep) {
			r->content_length = 0;
			r->transfer_chunked = 0;
			break;
		}

		/* empty chunk == eof */
		if (!r->content_length) {
			r->transfer_chunked = false;
			break;
		}
	}

	buf = ustream_get_read_buf(cl->us, &len);
	if (!r->content_length && !r->transfer_chunked &&
		cl->state != CLIENT_STATE_DONE) {
		if (cl->dispatch.data_done)
			cl->dispatch.data_done(cl);

		cl->state = CLIENT_STATE_DONE;
	}
}

static bool client_data_cb(struct client *cl, char *buf, int len)
{
	client_poll_post_data(cl);
	return false;
}

static bool client_header_cb(struct client *cl, char *buf, int len)
{
	char *newline;
	int line_len;

	newline = strstr(buf, "\r\n");
	if (!newline)
		return false;

	*newline = 0;
	client_parse_header(cl, buf);
	line_len = newline + 2 - buf;
	ustream_consume(cl->us, line_len);
	if (cl->state == CLIENT_STATE_DATA)
		return client_data_cb(cl, newline + 2, len - line_len);

	return true;
}

typedef bool (*read_cb_t)(struct client *cl, char *buf, int len);
static read_cb_t read_cbs[] = {
	[CLIENT_STATE_INIT] = client_init_handler,
	[CLIENT_STATE_HEADER] = client_header_cb,
	[CLIENT_STATE_DATA] = client_data_cb,
};

void uh_client_read_cb(struct client *cl)
{
	struct ustream *us = cl->us;
	char *str;
	int len;

	client_done = false;
	do {
		str = ustream_get_read_buf(us, &len);
		if (!str || !len)
			break;

		if (cl->state >= array_size(read_cbs) || !read_cbs[cl->state])
			break;

		if (!read_cbs[cl->state](cl, str, len)) {
			if (len == us->r.buffer_len &&
			    cl->state != CLIENT_STATE_DATA)
				header_error(cl, 413, "Request Entity Too Large");
			break;
		}
	} while (!client_done);
}

static void client_close(struct client *cl)
{
	if (cl->refcount) {
		cl->state = CLIENT_STATE_CLEANUP;
		return;
	}

	client_done = true;
	n_clients--;
	dispatch_done(cl);
	uloop_timeout_cancel(&cl->timeout);
	if (cl->tls)
		uh_tls_client_detach(cl);
	ustream_free(&cl->sfd.stream);
	close(cl->sfd.fd.fd);
	list_del(&cl->list);
	blob_buf_free(&cl->hdr);
	free(cl);

	unblock_listeners();
}

void uh_client_notify_state(struct client *cl)
{
	struct ustream *s = cl->us;

	if (!s->write_error && cl->state != CLIENT_STATE_CLEANUP) {
		if (cl->state == CLIENT_STATE_DATA)
			return;

		if (!s->eof || s->w.data_bytes)
			return;
	}

	return client_close(cl);
}

static void client_ustream_read_cb(struct ustream *s, int bytes)
{
	struct client *cl = container_of(s, struct client, sfd.stream);

	uh_client_read_cb(cl);
}

static void client_ustream_write_cb(struct ustream *s, int bytes)
{
	struct client *cl = container_of(s, struct client, sfd.stream);

	if (cl->dispatch.write_cb)
		cl->dispatch.write_cb(cl);
}

static void client_notify_state(struct ustream *s)
{
	struct client *cl = container_of(s, struct client, sfd.stream);

	uh_client_notify_state(cl);
}

static void set_addr(struct uh_addr *addr, void *src)
{
	struct sockaddr_in *sin = src;
	struct sockaddr_in6 *sin6 = src;

	addr->family = sin->sin_family;
	if (addr->family == AF_INET) {
		addr->port = ntohs(sin->sin_port);
		memcpy(&addr->in, &sin->sin_addr, sizeof(addr->in));
	} else {
		addr->port = ntohs(sin6->sin6_port);
		memcpy(&addr->in6, &sin6->sin6_addr, sizeof(addr->in6));
	}
}

bool uh_accept_client(int fd, bool tls)
{
	static struct client *next_client;
	struct client *cl;
	unsigned int sl;
	int sfd;
	static int client_id = 0;
	struct sockaddr_in6 addr;

	if (!next_client)
		next_client = calloc(1, sizeof(*next_client));

	cl = next_client;

	sl = sizeof(addr);
	sfd = accept(fd, (struct sockaddr *) &addr, &sl);
	if (sfd < 0)
		return false;

	set_addr(&cl->peer_addr, &addr);
	sl = sizeof(addr);
	getsockname(sfd, (struct sockaddr *) &addr, &sl);
	set_addr(&cl->srv_addr, &addr);

	cl->us = &cl->sfd.stream;
	if (tls) {
		uh_tls_client_attach(cl);
	} else {
		cl->us->notify_read = client_ustream_read_cb;
		cl->us->notify_write = client_ustream_write_cb;
		cl->us->notify_state = client_notify_state;
	}

	cl->us->string_data = true;
	ustream_fd_init(&cl->sfd, sfd);

	poll_connection(cl);
	list_add_tail(&cl->list, &clients);

	next_client = NULL;
	n_clients++;
	cl->id = client_id++;
	cl->tls = tls;

	return true;
}

void uh_close_fds(void)
{
	struct client *cl;

	uloop_done();
	close_listeners();
	list_for_each_entry(cl, &clients, list) {
		close(cl->sfd.fd.fd);
		if (cl->dispatch.close_fds)
			cl->dispatch.close_fds(cl);
	}
}
