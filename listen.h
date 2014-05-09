/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: listen.h
 * Description: the listener socket functionality
 *
 * Created by: Daan Pape
 * Created on: May 9, 2014
 */

#ifndef LISTEN_H_
#define LISTEN_H_

#include <stdbool.h>

/**
 * Bind a socket to listen from request on a given host on a given host.
 * @host the host to bind the socket to, NULL for any host
 * @port the port to listen to
 * @tls true if this socket sould listen for TLS connections
 */
bool bind_listener_sockets(const char *host, const char *port, bool tls);

/**
 * Setup all listeners in the listener list and
 * bind them to the uloop event system
 */
void setup_listeners(void);

#endif /* LISTEN_H_ */
