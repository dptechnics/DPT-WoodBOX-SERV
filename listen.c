/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: listen.c
 * Description: the listener socket functionality
 *
 * Created by: Daan Pape
 * Created on: May 9, 2014
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdbool.h>

#include "uhttpd.h"

/**
 * Listener structure
 */
struct listener {
	struct list_head list;
	struct uloop_fd fd;
	int socket;
	int n_clients;
	struct sockaddr_in6 addr;
	bool tls;
	bool blocked;
};

static LIST_HEAD(listeners);
static int n_blocked;

void uh_close_listen_fds(void)
{
	struct listener *l;

	list_for_each_entry(l, &listeners, list)
		close(l->fd.fd);
}

static void uh_block_listener(struct listener *l)
{
	uloop_fd_delete(&l->fd);
	n_blocked++;
	l->blocked = true;
}

static void uh_poll_listeners(struct uloop_timeout *timeout)
{
	struct listener *l;

	if ((!n_blocked && conf.max_connections) ||
	    n_clients >= conf.max_connections)
		return;

	list_for_each_entry(l, &listeners, list) {
		if (!l->blocked)
			continue;

		l->fd.cb(&l->fd, ULOOP_READ);
	    if (n_clients >= conf.max_connections)
			break;

		n_blocked--;
		l->blocked = false;
		uloop_fd_add(&l->fd, ULOOP_READ);
	}
}

void uh_unblock_listeners(void)
{
	static struct uloop_timeout poll_timer = {
		.cb = uh_poll_listeners
	};

	uloop_timeout_set(&poll_timer, 1);
}

static void listener_cb(struct uloop_fd *fd, unsigned int events)
{
	struct listener *l = container_of(fd, struct listener, fd);

	while (1) {
		if (!uh_accept_client(fd->fd, l->tls))
			break;
	}

	if (conf.max_connections && n_clients >= conf.max_connections)
		uh_block_listener(l);
}

void uh_setup_listeners(void)
{
	struct listener *l;
	int yes = 1;

	list_for_each_entry(l, &listeners, list) {
		int sock = l->fd.fd;

		/* TCP keep-alive */
		if (conf.tcp_keepalive > 0) {
#ifdef linux
			int tcp_ka_idl, tcp_ka_int, tcp_ka_cnt;

			tcp_ka_idl = 1;
			tcp_ka_cnt = 3;
			tcp_ka_int = conf.tcp_keepalive;

			setsockopt(sock, SOL_TCP, TCP_KEEPIDLE,  &tcp_ka_idl, sizeof(tcp_ka_idl));
			setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &tcp_ka_int, sizeof(tcp_ka_int));
			setsockopt(sock, SOL_TCP, TCP_KEEPCNT,   &tcp_ka_cnt, sizeof(tcp_ka_cnt));
#endif

			setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
		}

		l->fd.cb = listener_cb;
		uloop_fd_add(&l->fd, ULOOP_READ);
	}
}


/**
 * Bind a socket to listen from request on a given host on a given host.
 * @host the host to bind the socket to, NULL for any host
 * @port the port to listen to
 * @tls true if this socket sould listen for TLS connections
 */
bool bind_listener_socket(const char *host, const char *port, bool tls)
{
	int sock = -1;
	int yes = 1;
	int status;
	int bound = 0;
	struct listener *l = NULL;
	struct addrinfo *addrs = NULL, *p = NULL;
	static struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	};

	if ((status = getaddrinfo(host, port, &hints, &addrs)) != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(status));
		return false;
	}

	/* try to bind a new socket to each found address */
	for (p = addrs; p; p = p->ai_next) {
		/* get the socket */
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock < 0) {
			perror("socket()");
			goto error;
		}

		/* "address already in use" */
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
			perror("setsockopt()");
			goto error;
		}

		/* required to get parallel v4 + v6 working */
		if (p->ai_family == AF_INET6 &&
		    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
			perror("setsockopt()");
			goto error;
		}

		/* bind */
		if (bind(sock, p->ai_addr, p->ai_addrlen) < 0) {
			perror("bind()");
			goto error;
		}

		/* listen */
		if (listen(sock, UH_LIMIT_CLIENTS) < 0) {
			perror("listen()");
			goto error;
		}

		fd_cloexec(sock);

		l = calloc(1, sizeof(*l));
		if (!l)
			goto error;

		l->fd.fd = sock;
		l->tls = tls;
		list_add_tail(&l->list, &listeners);
		bound++;

		continue;

error:
		if (sock > -1)
			close(sock);
		return false;
	}

	freeaddrinfo(addrs);

	return true;
}
