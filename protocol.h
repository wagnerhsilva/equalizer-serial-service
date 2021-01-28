/*
 * protocol.h
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_
#define CCK_ZERO_DEBUG_V(r) {_cck_zero_debug_vars((r), __LINE__, __FILE__);}
#define CCK_ZERO_DEBUG_E(r) {_cck_zero_debug_impe((r), __LINE__, __FILE__);}
#define CCK_ZERO_DEBUG_F(r, index) {_cck_zero_debug_vars_f((r), index, __LINE__, __FILE__);}
#include <serial.h>

typedef struct {
	unsigned char	addr_bank;
	unsigned char	addr_batt;
	unsigned short	vref;
	unsigned short	duty_min;
	unsigned short	duty_max;
	unsigned short  index;
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
	unsigned short  currentRead; // Adicionada variavel para de leitura de corrente
	unsigned char   currentOrientation; // Adicionada variavel para leitura do sentido da corrente
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

typedef struct {
	unsigned int temperatura;
	unsigned int tensao;
	unsigned int impedancia;
	unsigned int barramento;
	unsigned int target;
	unsigned int disk;
	unsigned int corrente;
} Protocol_States;

typedef enum {
	TENSAO,
	TEMPERATURA,
	IMPEDANCIA,
	BARRAMENTO,
	TARGET,
	DISK,
	STRING, 
	CORRENTE
} Protocol_States_e;

int prot_init(Serial_t *serial);
int prot_read_vars(Protocol_ReadCmd_InputVars *in, Protocol_ReadCmd_OutputVars *out, int retries);
int prot_read_impedance(Protocol_ImpedanceCmd_InputVars *in, Protocol_ImpedanceCmd_OutputVars *out, int retries);
void prot_ext_print_info(Protocol_ReadCmd_OutputVars *vars);

int _cck_zero_debug_vars(Protocol_ReadCmd_OutputVars *out, int line, const char *file);
int _cck_zero_debug_impe(Protocol_ImpedanceCmd_InputVars *in, int line, const char *file);
int _cck_zero_debug_vars_f(Protocol_ReadCmd_OutputVars *out, int index, int line, const char *file);

#endif /* PROTOCOL_H_ */
