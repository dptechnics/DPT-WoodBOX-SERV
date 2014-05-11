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

#include <libubox/uloop.h>
#include <libubox/usock.h>

#include "listen.h"
#include "main.h"
#include "config.h"
#include "uhttpd.h"
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

	/* Initialize network event loop */
	uloop_init();

	/* Set up all listener sockets */
	setup_listeners();

	/* Start the network event loop */
	uloop_run();

	return EXIT_SUCCESS;
}

/**
 * Create the server configuration and bind to socket.
 * @return: true if configuration was successful.
 */
bool load_configuration(void)
{
	/* Add cgi dispatch handler */
	uh_dispatch_add(&api_dispatch);

	/* Set up configuration */
	conf.script_timeout = 60;
	conf.max_script_requests = 3;
	conf.max_connections = 100;
	conf.realm = "WoodBox Secured";
	conf.cgi_docroot_path = "/www/api";
	conf.cgi_path = "/sbin:/usr/sbin:/bin:/usr/bin";

	/* Bind a non TLS socket to port 80 */
	if(!bind_listener_sockets(NULL, "80", false)) {
		fprintf(stderr, "[ERROR] Could not bind socket to 0.0.0.0:80\n");
		return false;
	}

	return true;
}




