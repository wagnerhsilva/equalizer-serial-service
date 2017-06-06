/*
 * service.h
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#ifndef SERVICE_H_
#define SERVICE_H_

#include <database.h>
#include <protocol.h>
#include <serial.h>
#include <disk.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>

int service_init(char *dev_path, char *db_path);
int service_start(void);
int service_stop(void);
int service_finish(void);

#endif /* SERVICE_H_ */
