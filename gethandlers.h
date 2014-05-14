/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: gethandlers.h
 * Description: the main program
 *
 * Created by: Daan Pape
 * Created on: May 14, 2014
 */

#ifndef GETHANDLERS_H_
#define GETHANDLERS_H_

#include <json/json.h>

#include "uhttpd.h"

/**
 * Get free disk space if a mounted filesystem
 * could be found.
 * @cl the client who made the request
 */
json_object* get_free_disk_space(struct client *cl);

#endif
