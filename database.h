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
	unsigned short  index;
} Database_Address_t;

typedef struct {
	unsigned short	average_last;
	unsigned short	duty_min;
	unsigned short	duty_max;
	unsigned short  index;
	unsigned short  delay;
	unsigned short  num_cycles_var_read;
	unsigned int	bus_sum; /* param 1 */
	unsigned int	save_log_time;
	unsigned int	disk_capacity; /* porcentagem ocupada do disco */
	unsigned int	num_banks; /* para calcular tensao barramento */
	unsigned int	param1_interbat_delay; /* param1 - intervalo, em microssegundos, entre leitura de bateria */
	unsigned int	param2_serial_read_to; /* param2 - tempo de timeout, em microssegundos, de esperar de resposta do sensor pela serial */
	unsigned int	param3_messages_wait;
} Database_Parameters_t;

typedef struct {
	int temperatura_min;
	int temperatura_max;
	unsigned int tensao_min;
	unsigned int tensao_max;
	unsigned int impedancia_min;
	unsigned int impedancia_max;
} Database_Alarmconfig_t;

typedef struct {
	int items;
	Database_Address_t item[DATABASE_MAX_ADDRESSES_LEN];
} Database_Addresses_t;

int db_init(char *path);
int db_finish(void);
int db_add_vars(Protocol_ReadCmd_OutputVars *vars);
int db_add_impedance(unsigned char addr_bank, unsigned char addr_batt, Protocol_ImpedanceCmd_OutputVars *vars);

int db_add_response(
		Protocol_ReadCmd_OutputVars *read_vars,
		Protocol_ImpedanceCmd_OutputVars *imp_vars,
		Protocol_States *states,
		int id_db,
		int save_log);
int db_add_alarm(Protocol_ReadCmd_OutputVars *read_vars,
		Protocol_ImpedanceCmd_OutputVars *imp_vars,
		Protocol_States *states,
		Database_Alarmconfig_t *alarmconfig,
		Protocol_States_e tipo);
int db_get_addresses(Database_Addresses_t *list,Database_Parameters_t *p_list);
int db_get_parameters(Database_Parameters_t *list, Database_Alarmconfig_t *alarmconfig);
int db_set_macaddress(void);
int db_update_average(unsigned short new_avg, unsigned int new_sum, unsigned int capacity);

#endif /* DATABASE_H_ */
