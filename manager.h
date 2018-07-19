#ifndef _CM_STRINGS_MANAGER_
#define _CM_STRINGS_MANAGER_

#include <component.h>

/*
 * This is a Manager component for the CM-Strings
 * so we can manipulate the hole bank with a single instance
*/

typedef struct{
	cm_string_t **cm_strings; //List of CM-Strings
	int count; //amount of CM-Strings
	int batteries; //amount of batteries (per CM-String)
	int items; //amount of batteries (total)
}cm_manager_t;

/*
 * Creates a new manager, assumes *manager is nullptr
*/
bool cm_manager_new(cm_manager_t **manager);

/*
 * handles the size of the cm_strings component of the 'manager' so that
 * it can fit 'strings' amount of CM-Strings and 'batteries' amount of batteries
*/
bool cm_manager_setup(cm_manager_t **manager, int strings, int batteries);

/*
 * clears a instance of the cm_manager_t structure
*/
bool cm_manager_destroy(cm_manager_t **manager);

/*
 * Reads all the CM-Strings present in the cm_manager_t 'manager'.
 * The arguments are passed to the API for the CM-String to perform read operations
*/
bool cm_manager_read_strings(cm_manager_t *manager, bool firstRead,
 							 Database_Parameters_t params, const Read_t type, bool *stringStatus);

/*
 * Process batteries for alarms. This actually relies on the CM-String API for the 
 * alarm generation. No process is done on this API.
*/
bool cm_manager_process_batteries(cm_manager_t *manager, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead, int was_global_read_ok);

/*
 * Process strings for alarms. This actually relies on the CM-String API for the alarm generation
 * and it is not done by the manager.
*/
bool cm_manager_process_strings(cm_manager_t *manager,  Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead);

/*
 * Get the amount of CM-Strings handled by the manager 'manager'
*/
int cm_manager_string_count(cm_manager_t *manager);

/*
 * Get the amount of batteries (total) handled by the manager 'manager'
*/
int cm_manager_batteries_count(cm_manager_t *manager);

/*
 * Get the amount of batteries (per string) handled by the manager 'manager'
*/
int cm_manager_batteries_per_string_count(cm_manager_t *manager);

#endif