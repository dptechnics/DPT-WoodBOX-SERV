/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: config.h
 * Description: the woodbox server configuration header file
 *
 * Created by: Daan Pape
 * Created on: May 9, 2014
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define FORK_ON_START 			false			/* True if the server should fork on startup */
#define WORKING_BUFF_SIZE		4096			/* Size of the working buffer, should be smaller than PAGE_MAX */

#define KEEP_ALIVE_TIME			20				/* Time in seconds for Keep-Alive connections */
#define NETWORK_TIMEOUT			30				/* The number of seconds before timeout is detected */
#define INDEX_FILE				"index.html"	/* The default index page */
#define DOCUMENT_ROOT			"/www"			/* The document root */

#endif
