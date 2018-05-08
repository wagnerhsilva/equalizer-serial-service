#ifndef _CM_STRINGS_MANAGER_
#define _CM_STRINGS_MANAGER_

#include <component.h>

typedef struct{
	cm_string_t **cm_strings;
	int count;
	int batteries;
	int items;
}cm_manager_t;

bool cm_manager_new(cm_manager_t **manager);

bool cm_manager_setup(cm_manager_t **manager, int strings, int batteries);

bool cm_manager_destroy(cm_manager_t **manager);

bool cm_manager_read_strings(cm_manager_t *manager, bool firstRead,
 							 Database_Parameters_t params, const Read_t type);

bool cm_manager_process_batteries(cm_manager_t *manager, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead);

bool cm_manager_process_strings(cm_manager_t *manager,  Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead);

int cm_manager_string_count(cm_manager_t *manager);

int cm_manager_batteries_count(cm_manager_t *manager);

int cm_manager_batteries_per_string_count(cm_manager_t *manager);

#endif