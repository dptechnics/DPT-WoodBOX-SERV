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

#include <mntent.h>
#include <sys/vfs.h>
#include <json/json.h>

#include "uhttpd.h"
#include "gethandlers.h"

/**
 * Get free disk space if a mounted filesystem
 * could be found.
 * @cl the client who made the request
 */
json_object* get_free_disk_space(struct client *cl)
{
	/* The mount point we want to check */
	struct statfs s;
	statfs("/overlay", &s);

	/* Create test json object */
	json_object *jobj = json_object_new_object();
	json_object *usedspace = json_object_new_int((int)((s.f_bavail * s.f_frsize)/1024));
	json_object *totalspace = json_object_new_int((int)((s.f_blocks * s.f_frsize)/1024));

	json_object_object_add(jobj, "used", usedspace);
	json_object_object_add(jobj, "total", totalspace);

	/* Return status ok */
	cl->http_status = r_ok;
	return jobj;
}
