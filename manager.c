#include <manager.h>

#define nullptr NULL
/*
 * checks if a pointer is initialized or not, immediatly returns false if not
*/
#define PTR_VALID(x) { if(!(x)){ printf("MANAGER:Not a valid pointer[%s]::%d\n", (char *)#x, __LINE__); return false; }}

/*
 * make a pointer attribution, y should be the address of the pointer
 * and x is the operation. To be used with (m,re,c)alloc functions
 * immediatly returns false if the operation failed
*/
#define PTR_ATT(y, x) { (*y) = (x); if((*y) == nullptr) { printf("MANAGER:No Memory for %s=%s::%d\n", (char *)#y, (char*)#x, __LINE__); return false; } }


/*
 * Creates a new instance of a manager, it assumes *manager apoints to nullptr
*/
bool cm_manager_new(cm_manager_t **manager){
	PTR_VALID(manager);
	/*
	 * If this already has a manager, do not touch it
	*/
	if(*manager){
		printf("Already has a manager!\n");
		return false;
	}
	/* Allocate a pointer to CM-Strings and set values to zero */
	PTR_ATT(&(*manager), (cm_manager_t *)malloc(sizeof(cm_manager_t)));

	(*manager)->count = 0;
	(*manager)->cm_strings = 0;
	(*manager)->batteries = 0;

	return true;
}

/*
 * Destroys all CM-Strings present in the manager 'manager'
 * and free the pointer of CM-Strings in the 'manager'
*/
static bool clear_strings(cm_manager_t **manager){
	for(int i = 0; i < (*manager)->count; i+= 1){
		cm_string_destroy(&((*manager)->cm_strings[i]));
	}

	if((*manager)->cm_strings){
		free((*manager)->cm_strings);
	}
}

/*
 * Allocates memory for the CM-Strings the manager 'manager' needs to handle
 * given by 'strings' CM-Strings and 'batteries' batteries per string.
*/
bool cm_manager_setup(cm_manager_t **manager, int strings, int batteries){
	PTR_VALID(manager);
	PTR_VALID(*manager);

	/*
	 * Inform if we have reset the values
	*/
	bool changed = false;
	/* Only if the number of components is different */
	if((*manager)->count != strings || (*manager)->batteries != batteries){
		LOG("Manager:Trocando quantidade de strings/baterias\n");
		clear_strings(manager);

		/* Allocate memory for every single CM-String */
		PTR_ATT(&((*manager)->cm_strings), (cm_string_t **)malloc(sizeof(cm_string_t *) * strings));

		/* Instaciate all CM-Strings and set their id so we can correctly save them in the database */
		for(int i  = 0; i < strings; i += 1){
			(*manager)->cm_strings[i] = 0;
			cm_string_new(&((*manager)->cm_strings[i]), batteries);
			cm_string_set_id(&((*manager)->cm_strings[i]), i);
		}

		(*manager)->count = strings;
		(*manager)->batteries = batteries;
		(*manager)->items = strings * batteries;
		changed = true;
	}

	return changed;
}

/*
 * Process baterries, actually just repass the call to every CM-String
*/
bool cm_manager_process_batteries(cm_manager_t *manager, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead, int was_global_read_ok)
{
	PTR_VALID(manager);
	bool ret = true;
	for(int i = 0; i < manager->count; i+= 1){
		ret &= cm_string_process_batteries(manager->cm_strings[i], alarmconfig,
										   params, save_log_state, firstRead,
										   was_global_read_ok);
	}
	return ret;
}

/*
 * Process the strings, actually just repass the call to every CM-String
*/
bool cm_manager_process_strings(cm_manager_t *manager,  Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead)
{
	PTR_VALID(manager);
	bool ret = true;
	for(int i = 0; i < manager->count; i+= 1){
		ret &= cm_string_process_string_alarms(manager->cm_strings[i], alarmconfig,
											   params, capacity, firstRead);
	}

	return ret;
}

/*
 * Call read function for every CM-String
*/
bool cm_manager_read_strings(cm_manager_t *manager,
						  bool firstRead,
						  Database_Parameters_t params,
						  const Read_t type,
						  bool *stringStatus)
{
	PTR_VALID(manager);
	bool ret = false;
	for(int i = 0; i < manager->count; i+= 1){
		LOG("Manager:Reading string %d\n", cm_string_get_id(manager->cm_strings[i]));
		bool status = cm_string_do_read_all(manager->cm_strings[i], type, params, firstRead);
		*stringStatus &= status;
		ret |= status;
		if(!status){
			printf("Failed to read string %d\n", i);
		}
	}

	return ret;
}

bool cm_manager_destroy(cm_manager_t **manager){
	PTR_VALID(manager);
	PTR_VALID(*manager);

	clear_strings(manager);
	free(*manager);
	manager = 0;
	return true;
}

int cm_manager_string_count(cm_manager_t *manager){
	PTR_VALID(manager);
	return manager->count;
}

int cm_manager_batteries_count(cm_manager_t *manager){
	PTR_VALID(manager);
	return manager->items;
}

int cm_manager_batteries_per_string_count(cm_manager_t *manager){
	PTR_VALID(manager);
	return manager->batteries;
}