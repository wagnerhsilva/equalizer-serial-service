#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include <database.h>
#include <protocol.h>
#include <serial.h>
#include <disk.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>
#include <bits.h>

/*
 * This component is a representation of a CM-String which is 
 * basically a list of batteries. Here we persist data for 
 * each battery so that we can compute alarms and fill
 * the database for the Web part of this application.
*/

/*
 * Enum representing which read is to be made,
 * VARS = usual variables from batteries,
 * IMPE = read impedance values from batteries
*/
typedef enum{
	VARS=0, IMPE=1
}Read_t;

/*
 * Representation of a CM-String
*/
typedef struct{
	Protocol_ReadCmd_OutputVars 		*output_vars_read_last; //persisted last read of VARS
	Protocol_ReadCmd_OutputVars 		*output_vars_read_curr; //current read of VARS
	
	Protocol_ImpedanceCmd_OutputVars 	*output_vars_imp_last; //persisted last read of IMPE
	Protocol_ImpedanceCmd_OutputVars 	*output_vars_imp_curr; //current read of IMPE

	int 								*batteries_read_states_curr; //state of the batteries (amount of reads that failed)
	
	Protocol_States						*batteries_states_curr; //current alarms generated by each battery
	Protocol_States						*batteries_states_last; //last alarms generated by each battery



	int 								*batteries_has_read; //boolean indicating if battery has a read

	StringAvg_t							average_vars_curr; //current average values for this CM-String
	StringAvg_t							average_vars_last; //last average values for this CM-String

	int 								string_id; //id of the string (0, 1, 2, ...) = (S1, S2, S3, ...)
	int 								string_size; //size of the string (amount of batteries)
	bool								is_inited; //boolean indicating if this structure was inited

	bool 								string_ok; //flag indicating if any battery failed a read for this CM-String
	Bits 								battery_mask; //bitmask so we can easily get which battery failed

}cm_string_t;

/*
 * Returns the id of the CM-String 'str'
*/
int  cm_string_get_id(cm_string_t *str);

/*
 * Sets the id of the CM-String 'str' to 'id'
*/
bool cm_string_set_id(cm_string_t **str, int id);

/*
 * Creates a new CM-String with a fixed 'size' batteries
*/
bool cm_string_new(cm_string_t **str_t, int size);

/*
 * Read all batteries present in the CM-String 'str_t', read type is 'type' (VARS or IMPE)
 * to read either vars or impedance. 'params' controlls timeouts used in this read. If
 * 'firstRead' is set to true than this call also read the complementary of 'type', i.e.:
 * a call to cm_string_do_read_all(..., ..., VARS, params, true) will read VARS and then 
 * IMPE in the same call
*/
bool cm_string_do_read_all(cm_string_t *str_t, const Read_t type,
 						   Database_Parameters_t params, bool firstRead);

/*
 * Process each battery in the CM-String 'str' based on their last read. It uses 'alarmconfig'
 * to define if any property for this battery is in alarm or not. In case it is it creates a 
 * query using the API defined in database.h to write to database. 'firstRead' controlls
 * the actual write to database, i.e.: in case true it does not write to alarm table
 * only compute the state of the batteries. 'save_log_state' defines if the log messages
 * should be saved.
*/
bool cm_string_process_batteries(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead, int was_global_read_ok);

bool cm_string_process_eval_tendencies(int was_global_read_ok);
bool cm_string_process_save_tendencies(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead, int was_global_read_ok);
bool cm_string_process_update_tendencies(void);

/*
 * Process the CM-String 'str' to treat alarms related to it. Considers the 'alarmconfig'
 * to define if an alarm is happening or not, it also writes the capacity of disk as given 
 * by 'capacity'. 'firstRead' controlls if the alarms should be written to the database 
 * or only be computed.
*/
bool cm_string_process_string_alarms(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead);

/*
 * Clears a CM-String given by 'str_t', once returned 
 * (*str_t) will be pointing to null
*/
bool cm_string_destroy(cm_string_t **str_t);
#endif