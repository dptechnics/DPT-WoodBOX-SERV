/*
 * WoodBOX-server
 *
 * Server appliction for the DPTechnics WoodBOX.
 *
 * File: proc.h
 * Description: the process interface
 *
 * Created by: Daan Pape
 * Created on: May 11, 2014
 */

#ifndef PROC_H_
#define PROC_H_

/**
 * Parse the client request into process variables.
 * @cl the client who made the request
 * @pi info about the requeste path
 */
struct env_var *get_process_vars(struct client *cl, struct path_info *pi);

#endif
