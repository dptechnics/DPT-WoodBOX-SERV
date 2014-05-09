/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: main.c
 * Description: the main program
 *
 * Created by: Daan Pape
 * Created on: May 9, 2014
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

#include "main.h"
#include "config.h"
#include "uhttpd.h"
#include "tls.h"

/**
 * The servers main working buffer.
 */
char uh_buf[WORKING_BUFF_SIZE];

/**
 * Main application entry point.
 * @argc the number of command line arguments.
 * @argv the command line arguments.
 */
int main(int argc, char **argv)
{
	/* Current file descriptor for /dev/null */
	int cur_fd;

	/* Prevent SIGPIPE errors */
	signal(SIGPIPE, SIG_IGN);

	/* Load default configuration */
	if(!load_configuration()){
		return EXIT_FAILURE;
	}

	/* fork (if not disabled) */
	if (FORK_ON_START) {
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

	/* Set up uloop event loop */
	uloop_init();
	uh_setup_listeners();
	uh_plugin_post_init();
	uloop_run();

	return EXIT_SUCCESS;
}

/**
 * Create the server configuration and bind to socket.
 * @return: true if configuration was successful.
 */
bool load_configuration(void)
{
	/* Set up configuration */
	conf.script_timeout = 60;
	conf.network_timeout = 30;
	conf.http_keepalive = 20;
	conf.max_script_requests = 3;
	conf.max_connections = 100;
	conf.realm = "WoodBox Secured";
	conf.cgi_prefix = "/api";
	conf.cgi_path = "/sbin";
	conf.docroot = "/www";

	/* Bind a non TLS socket to port 80 */
	if(!uh_socket_bind(NULL, "80", false)) {
		fprintf(stderr, "[ERROR] Could not bind socket to 0.0.0.0:80\n");
		return false;
	}

	/* Make 'index.html' the default document */
	uh_index_add("index.html");

	return true;
}




