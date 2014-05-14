/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: gethandlers.c
 * Description: all the http get handlers are defined
 * in this file
 *
 * Created by: Daan Pape
 * Created on: May 14, 2014
 */

#include "uhttpd.h"
#include "gethandlers.h"

/**
 * Get free disk space if a mounted filesystem
 * could be found.
 * @cl the client who made the request
 */
void get_free_disk_space(struct client *cl)
{
	char* test = "{\"used\":334,\"total\":553534}";
	cl->response = (char*) malloc((strlen(test)+1)*sizeof(char));
	strcpy(cl->response, test);
}
