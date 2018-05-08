#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include <database.h>
#include <protocol.h>
#include <serial.h>
#include <disk.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>

typedef enum{
	VARS=0, IMPE=1
}Read_t;

typedef struct{
	Protocol_ReadCmd_OutputVars 		*output_vars_read_last;
	Protocol_ReadCmd_OutputVars 		*output_vars_read_curr;
	
	Protocol_ImpedanceCmd_OutputVars 	*output_vars_imp_last;
	Protocol_ImpedanceCmd_OutputVars 	*output_vars_imp_curr;

	int 								*batteries_read_states_last;
	int 								*batteries_read_states_curr;
	
	Protocol_States						*batteries_states_curr;
	Protocol_States						*batteries_states_last;



	int 								*batteries_has_read;

	StringAvg_t							average_vars_curr;
	StringAvg_t							average_vars_last;

	int 								string_id;
	int 								string_size;
	bool								is_inited;

	bool 								string_ok;
	int 								last_batt_err;
	int 								last_batt_num;

}cm_string_t;


bool cm_string_set_id(cm_string_t **str, int id);

bool cm_string_new(cm_string_t **str_t, int size);

bool cm_string_set_size(cm_string_t **str_t, int size);

bool cm_string_do_read_all(cm_string_t *str_t, const Read_t type,
 						   Database_Parameters_t params, bool firstRead);

bool cm_string_process_batteries(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead);

bool cm_string_process_string_alarms(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead);

bool cm_string_destroy(cm_string_t **str_t);
#endif