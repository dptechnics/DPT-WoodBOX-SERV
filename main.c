/*
 * uhttpd - Tiny single-threaded httpd
 *
 *   Copyright (C) 2010-2013 Jo-Philipp Wich <xm@subsignal.org>
 *   Copyright (C) 2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _BSD_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE	700
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <getopt.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

#include <libubox/usock.h>

#include "uhttpd.h"
#include "tls.h"

/**
 * The server write and receive buffer
 */
char uh_buf[4096];

/**
 * Start the woodbox server loop
 * @return exit succes
 */
static int run_server(void)
{
	uloop_init();
	uh_setup_listeners();
	uh_plugin_post_init();
	uloop_run();

	return 0;
}

/**
 * Parse configuration file 
 */
static void uh_config_parse(void)
{
	const char *path = conf.file;
	FILE *c;
	char line[512];
	char *col1;
	char *col2;
	char *eol;

        /* Specify default path */
	if (!path) {
                path = "/etc/config/woodbox-server";
        }

	c = fopen(path, "r");
	if (!c)
		return;

	memset(line, 0, sizeof(line));

	while (fgets(line, sizeof(line) - 1, c)) {
		if ((line[0] == '/') && (strchr(line, ':') != NULL)) {
			if (!(col1 = strchr(line, ':')) || (*col1++ = 0) ||
				!(col2 = strchr(col1, ':')) || (*col2++ = 0) ||
				!(eol = strchr(col2, '\n')) || (*eol++  = 0))
				continue;

			uh_auth_add(line, col1, col2);
		} else if (!strncmp(line, "I:", 2)) {
			if (!(col1 = strchr(line, ':')) || (*col1++ = 0) ||
				!(eol = strchr(col1, '\n')) || (*eol++  = 0))
				continue;

			uh_index_add(strdup(col1));
		} else if (!strncmp(line, "E404:", 5)) {
			if (!(col1 = strchr(line, ':')) || (*col1++ = 0) ||
				!(eol = strchr(col1, '\n')) || (*eol++  = 0))
				continue;

			conf.error_handler = strdup(col1);
		}
		else if ((line[0] == '*') && (strchr(line, ':') != NULL)) {
			if (!(col1 = strchr(line, '*')) || (*col1++ = 0) ||
				!(col2 = strchr(col1, ':')) || (*col2++ = 0) ||
				!(eol = strchr(col2, '\n')) || (*eol++  = 0))
				continue;

			uh_interpreter_add(col1, col2);
		}
	}

	fclose(c);
}

static int add_listener_arg(char *arg, bool tls)
{
	char *host = NULL;
	char *port = arg;
	char *s;
	int l;

	s = strrchr(arg, ':');
	if (s) {
		host = arg;
		port = s + 1;
		*s = 0;
	}

	if (host && *host == '[') {
		l = strlen(host);
		if (l >= 2) {
			host[l-1] = 0;
			host++;
		}
	}

	return uh_socket_bind(host, port, tls);
}

/**
 * Print command line usage to stdout
 * @return usage is only printed when wrong cli parameters
 * where passed to the server, so exit status 1.
 */
static int usage(void)
{
	fprintf(stderr,
		"Usage: woodbox-server -p [addr:]port -h docroot\n"
		"	-f              Do not fork to background\n"
		"	-c file         Configuration file, default is '/etc/httpd.conf'\n"
		"	-p [addr:]port  Bind to specified address and port, multiple allowed\n"
#ifdef HAVE_TLS
		"	-s [addr:]port  Like -p but provide HTTPS on this port\n"
		"	-C file         ASN.1 server certificate file\n"
		"	-K file         ASN.1 server private key file\n"
#endif
		"	-h directory    Specify the document root, default is '.'\n"
		"	-E string       Use given virtual URL as 404 error handler\n"
		"	-I string       Use given filename as index for directories, multiple allowed\n"
		"	-S              Do not follow symbolic links outside of the docroot\n"
		"	-D              Do not allow directory listings, send 403 instead\n"
		"	-R              Enable RFC1918 filter\n"
		"	-n count        Maximum allowed number of concurrent script requests\n"
		"	-N count        Maximum allowed number of concurrent connections\n"
#ifdef HAVE_UBUS
		"	-u string       URL prefix for UBUS via JSON-RPC handler\n"
		"	-U file         Override ubus socket path\n"
		"	-a              Do not authenticate JSON-RPC requests against UBUS session api\n"
#endif
		"	-t seconds      CGI, Lua and UBUS script timeout in seconds, default is 60\n"
		"	-T seconds      Network timeout in seconds, default is 30\n"
		"	-k seconds      HTTP keepalive timeout\n"
		"	-r string       Specify basic auth realm\n"
		"\n");
	return 1;
}

int main(int argc, char **argv)
{
	bool nofork = false;
	char *port;
	int opt, ch;
	int cur_fd;
	int bound = 0;

#ifdef HAVE_TLS
	int n_tls = 0;
	const char *tls_key = NULL, *tls_crt = NULL;
#endif
        /* Throw compilation error when buffer is large than max path size */
	BUILD_BUG_ON(sizeof(uh_buf) < PATH_MAX);
        
	uh_dispatch_add(&cgi_dispatch);
        
        /* Ignore SIGPIPE's */
	signal(SIGPIPE, SIG_IGN);
        
        /* Read server configuration */
	conf.script_timeout = 60;
	conf.network_timeout = 30;
	conf.http_keepalive = 20;
	conf.max_script_requests = 3;
	conf.max_connections = 100;
	conf.realm = "Protected Area";
	conf.cgi_prefix = "/api";
	conf.cgi_path = "/sbin:/usr/sbin:/bin:/usr/bin";
        
        while ((ch = getopt(argc, argv, "afSDRC:K:E:I:p:s:h:c:r:m:n:N:x:i:t:k:T:A:u:U:")) != -1) {
		switch(ch) {
#ifdef HAVE_TLS
		case 'C':
			tls_crt = optarg;
			break;

		case 'K':
			tls_key = optarg;
			break;

		case 's':
			n_tls++;
			/* fall through */
#else
		case 'C':
		case 'K':
		case 's':
			fprintf(stderr, "woodbox-server: TLS support not compiled, "
			                "ignoring -%c\n", opt);
			break;
#endif
		case 'p':
			optarg = strdup(optarg);
			bound += add_listener_arg(optarg, (ch == 's'));
			break;

		case 'h':
			if (!realpath(optarg, uh_buf)) {
				fprintf(stderr, "Error: Invalid directory %s: %s\n", optarg, strerror(errno));
				exit(1);
			}
			conf.docroot = strdup(uh_buf);
			break;

		case 'E':
			if (optarg[0] != '/') {
				fprintf(stderr, "Error: Invalid error handler: %s\n", optarg);
				exit(1);
			}
			conf.error_handler = optarg;
			break;

		case 'I':
			if (optarg[0] == '/') {
				fprintf(stderr, "Error: Invalid index page: %s\n",optarg);
				exit(1);
			}
			uh_index_add(optarg);
			break;

		case 'S':
			conf.no_symlinks = 1;
			break;

		case 'D':
			conf.no_dirlists = 1;
			break;

		case 'R':
			conf.rfc1918_filter = 1;
			break;

		case 'n':
			conf.max_script_requests = atoi(optarg);
			break;

		case 'N':
			conf.max_connections = atoi(optarg);
			break;
                        
		case 't':
			conf.script_timeout = atoi(optarg);
			break;

		case 'T':
			conf.network_timeout = atoi(optarg);
			break;

		case 'k':
			conf.http_keepalive = atoi(optarg);
			break;

		case 'A':
			conf.tcp_keepalive = atoi(optarg);
			break;

		case 'f':
			nofork = 1;
			break;

		/* basic auth realm */
		case 'r':
			conf.realm = optarg;
			break;

		/* config file */
		case 'c':
			conf.file = optarg;
			break;

#ifdef HAVE_UBUS
		case 'a':
			conf.ubus_noauth = 1;
			break;

		case 'u':
			conf.ubus_prefix = optarg;
			break;

		case 'U':
			conf.ubus_socket = optarg;
			break;
#else
		case 'a':
		case 'u':
		case 'U':
			fprintf(stderr, "woodbox-server: UBUS support not compiled, ignoring -%c\n", opt);
			break;
#endif
		default:
			return usage(argv[0]);
		}
	}
        
        /* Parse configuration file*/
	uh_config_parse();
        
        /* Configure document root */
	if (!conf.docroot) {
                if (!realpath(".", uh_buf)) {
			fprintf(stderr, "Error: Unable to determine work dir\n");
			return 1;
		}
		conf.docroot = strdup(uh_buf);
	}
        
        /* Set "index.html" as index file */
	uh_index_add("index.html");
        
        /* Check if server is bound to a port */
	if (!bound) {
		fprintf(stderr, "Error: No sockets bound, unable to continue\n");
		return 1;
	}

#ifdef HAVE_TLS
	if (n_tls) {
		if (!tls_crt || !tls_key) {
			fprintf(stderr, "Please specify a certificate and "
					"a key file to enable SSL support\n");
			return 1;
		}

		if (uh_tls_init(tls_key, tls_crt))
		    return 1;
	}
#endif

#ifdef HAVE_UBUS
	if (conf.ubus_prefix && uh_plugin_init("uhttpd_ubus.so"))
		return 1;
#endif

	/* fork (if not disabled) */
	if (!nofork) {
		switch (fork()) {
		case -1:
			perror("fork()");
			exit(1);

		case 0:
			/* daemon setup */
			if (chdir("/"))
				perror("chdir()");

			cur_fd = open("/dev/null", O_WRONLY);
			if (cur_fd > 0) {
				dup2(cur_fd, 0);
				dup2(cur_fd, 1);
				dup2(cur_fd, 2);
			}

			break;

		default:
			exit(0);
		}
	}
        
        /* Start running the server */
	return run_server();
}
