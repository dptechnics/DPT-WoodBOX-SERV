/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: main.h
 * Description: the main program
 *
 * Created by: Daan Pape
 * Created on: May 9, 2014
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdbool.h>

/**
 * Create the server configuration and bind to socket.
 * @return: true if configuration was successful.
 */
bool load_configuration(void);

#endif /* MAIN_H_ */
