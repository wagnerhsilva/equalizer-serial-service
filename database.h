/*
 * database.h
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#ifndef DATABASE_H_
#define DATABASE_H_

#include "protocol.h"

#define DATABASE_MAX_ADDRESSES_LEN		10240

typedef struct {
	unsigned char	addr_bank;
	unsigned char	addr_batt;
	unsigned short	vref;
	unsigned short	duty_min;
	unsigned short	duty_max;
} Database_Address_t;

typedef struct {
	int items;
	Database_Address_t item[DATABASE_MAX_ADDRESSES_LEN];
} Database_Addresses_t;

int db_init(char *path);
int db_finish(void);
int db_add_vars(Protocol_ReadCmd_OutputVars *vars);
int db_add_impedance(unsigned char addr_bank, unsigned char addr_batt, Protocol_ImpedanceCmd_OutputVars *vars);

int db_add_response(Protocol_ReadCmd_OutputVars *read_vars,
                    Protocol_ImpedanceCmd_OutputVars *imp_vars);
int db_get_addresses(Database_Addresses_t *list);

#endif /* DATABASE_H_ */
