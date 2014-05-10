/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: client.h
 * Description: all low level reading and writing occurs here.
 *
 * Created by: Daan Pape
 * Created on: May 10, 2014
 */

#ifndef CLIENT_H_
#define CLIENT_H_

/**
 * Write a http header to a client
 * @client the client to write the header to
 * @code the http status code t o write
 * @summary the http status code info, for example if code = 200, summary = "Ok"
 */
void write_http_header(struct client *cl, int code, const char *summary);

#endif /* CLIENT_H_ */
