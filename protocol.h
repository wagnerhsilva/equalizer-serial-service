/*
 * protocol.h
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <serial.h>

typedef struct {
	unsigned char	addr_bank;
	unsigned char	addr_batt;
	unsigned short	vref;
	unsigned short	duty_min;
	unsigned short	duty_max;
} Protocol_ReadCmd_InputVars;

typedef struct {
	unsigned short	errcode;
	unsigned short	vbat;
	unsigned short	itemp;
	unsigned short	etemp;
	unsigned short	vsource;
	unsigned char	hw_ver;
	unsigned char	fw_ver;
	unsigned short	vbat_off;
	unsigned short	ibat_off;
	unsigned short	vref;
	unsigned short	duty_cycle;
	unsigned char	addr_bank;
	unsigned char	addr_batt;
} Protocol_ReadCmd_OutputVars;

typedef struct {
	unsigned char	addr_bank;
	unsigned char	addr_batt;
} Protocol_ImpedanceCmd_InputVars;

typedef struct {
	unsigned short	errcode;
	unsigned int	impedance;
	unsigned int	current;
} Protocol_ImpedanceCmd_OutputVars;

int prot_init(Serial_t *serial);
int prot_read_vars(Protocol_ReadCmd_InputVars *in, Protocol_ReadCmd_OutputVars *out);
int prot_read_impedance(Protocol_ImpedanceCmd_InputVars *in, Protocol_ImpedanceCmd_OutputVars *out);

#endif /* PROTOCOL_H_ */
