/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: mimetimes.h
 * Description: contains all mimetypes used in the
 * woodbox server program.
 *
 * Created by: Daan Pape
 * Created on: 09/05/2014
 */

#include <stdlib.h>

#ifndef MIMETYPES_H
#define MIMETYPES_H

/**
 * Mimetype struct mapping a file extension to the corresponding
 * mimetype.
 */
struct mimetype {
	const char *extn;
	const char *mime;
};

/**
 * struct containing all the used mimetypes.
 */
static const struct mimetype uh_mime_types[] = {

	{ "js",      "text/javascript" },
	{ "css",     "text/css" },
	{ "html",    "text/html" },
	{ "json",    "application/json" },

	{ "png",     "image/png" },
	{ "jpg",     "image/jpeg" },

	{ NULL, NULL }
};

#endif

