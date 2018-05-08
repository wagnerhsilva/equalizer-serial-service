#include <component.h>

#define nullptr NULL
/*
 * One-liners, for simple verification
*/

/*
 * checks if a pointer is initialized or not, immediatly returns false if not
*/
#define PTR_VALID(x) { if(!(x)){ LOG("COMPONENT:Not a valid pointer[%s]::%d\n", (char *)#x, __LINE__); return false; }}

/*
 * make a pointer attribution, y should be the address of the pointer
 * and x is the operation. To be used with (m,re,c)alloc functions
 * immediatly returns false if the operation failed
*/
#define PTR_ATT(y, x) { (*y) = (x); if((*y) == nullptr) { LOG("COMPONENT:No Memory for %s=%s::%d\n", (char *)#y, (char*)#x, __LINE__); return false; } }


/*
 * Basic interface to handle CM-Strings operations
*/

bool cm_string_new(cm_string_t **str, int size){
	PTR_VALID(str);
	if(*str != NULL){
		printf("Tried to init already inited pointer!\n");
		return false;
	}
	PTR_ATT(&(*str), (cm_string_t *)malloc(sizeof(cm_string_t)));
	(*str)->is_inited = false;
	(*str)->string_size = -1;
	if(size > 0){
		(*str)->string_size = size;
		PTR_ATT(&((*str)->output_vars_read_last), 
			(Protocol_ReadCmd_OutputVars *)malloc(sizeof(Protocol_ReadCmd_OutputVars) * size));
		PTR_ATT(&((*str)->output_vars_read_curr),
			(Protocol_ReadCmd_OutputVars *)malloc(sizeof(Protocol_ReadCmd_OutputVars) * size));
		
		PTR_ATT(&((*str)->output_vars_imp_last), 
			(Protocol_ImpedanceCmd_OutputVars *)malloc(sizeof(Protocol_ImpedanceCmd_OutputVars) * size));
		PTR_ATT(&((*str)->output_vars_imp_curr),
			(Protocol_ImpedanceCmd_OutputVars *)malloc(sizeof(Protocol_ImpedanceCmd_OutputVars) * size));
		
		PTR_ATT(&((*str)->batteries_read_states_last),
			(int *)calloc(size, sizeof(int)));
		PTR_ATT(&((*str)->batteries_read_states_curr),
			(int *)calloc(size, sizeof(int)));
		
		PTR_ATT(&((*str)->batteries_states_curr),
			(Protocol_States *)malloc(sizeof(Protocol_States) * size));
		PTR_ATT(&((*str)->batteries_states_last),
			(Protocol_States *)malloc(sizeof(Protocol_States) * size));

		PTR_ATT(&((*str)->batteries_has_read),
			(int *)calloc(size, sizeof(int)));

		(*str)->string_ok = true;
		(*str)->is_inited = true;

	}
	return true;
}

bool cm_string_set_id(cm_string_t **str, int id){
	PTR_VALID(str);
	PTR_VALID(*str);
	(*str)->string_id = id;
	return true;
}

/*
 * NOT USED!
*/
bool cm_string_set_size(cm_string_t **str, int size){
	PTR_VALID(str);

	/*
	 * When scaling a buffer we consider the type of
	 * scaling. On scale downs realloc is around 40 times faster
	 * than free/malloc. However when scaling up realloc is 
	 * ~7600 times slower, so we call free/malloc.
	*/

	if((*str)->string_size > size){ //scale down
		PTR_ATT(&((*str)->output_vars_read_last), 
			(Protocol_ReadCmd_OutputVars *)realloc((*str)->output_vars_read_last, sizeof(Protocol_ReadCmd_OutputVars) * size));
		PTR_ATT(&((*str)->output_vars_read_curr),
			(Protocol_ReadCmd_OutputVars *)realloc((*str)->output_vars_read_curr, sizeof(Protocol_ReadCmd_OutputVars) * size));
		
		PTR_ATT(&((*str)->output_vars_imp_last), 
			(Protocol_ImpedanceCmd_OutputVars *)realloc((*str)->output_vars_imp_last, sizeof(Protocol_ImpedanceCmd_OutputVars) * size));
		PTR_ATT(&((*str)->output_vars_imp_curr),
			(Protocol_ImpedanceCmd_OutputVars *)realloc((*str)->output_vars_imp_curr, sizeof(Protocol_ImpedanceCmd_OutputVars) * size));
		
		PTR_ATT(&((*str)->batteries_read_states_last),
			(int *)realloc((*str)->batteries_read_states_last, sizeof(int) * size));
		PTR_ATT(&((*str)->batteries_read_states_curr),
			(int *)realloc((*str)->batteries_read_states_curr, sizeof(int) * size));
		
		PTR_ATT(&((*str)->batteries_states_curr),
			(Protocol_States *)realloc((*str)->batteries_states_curr, sizeof(Protocol_States) * size));
		PTR_ATT(&((*str)->batteries_states_last),
			(Protocol_States *)realloc((*str)->batteries_states_last, sizeof(Protocol_States) * size));

		PTR_ATT(&((*str)->batteries_has_read),
			(int *)realloc((*str)->batteries_has_read, size * sizeof(int)));

		memset((*str)->batteries_has_read, 1, sizeof(int) * size);
		(*str)->is_inited = true;
		(*str)->string_size = size;
		(*str)->string_ok = true;

	}else if((*str)->string_size < size){ //scale up
		bool ret = cm_string_destroy(str);
		ret &= cm_string_new(str, size);
		PTR_VALID(str); PTR_VALID(*str);
		(*str)->string_ok = true;
		memset((*str)->batteries_has_read, 1, sizeof(int) * size);
		(*str)->is_inited = true;
		return ret;
	}

	/*
	 * If there are no changes in size than we can just keep
	 * the buffer and return true
	*/

	return true;
}

static bool cm_string_read_one_vars(cm_string_t *str, int which, Database_Parameters_t params){
	Protocol_ReadCmd_InputVars 	 input_vars 	= {0};
	int err										= 0;

	input_vars.addr_bank = str->string_id + 1;
	input_vars.addr_batt = which + 1;

	input_vars.vref = str->average_vars_last.average;

	input_vars.duty_min = params.duty_min;
	input_vars.duty_max = params.duty_max;

	if(!str->string_ok){
		input_vars.duty_max = 0;
	}

	input_vars.index = params.index;
	err = prot_read_vars(&input_vars,
	 					 &(str->output_vars_read_curr[which]),
	 					 params.param3_messages_wait);
	if(err != 0){
		/*
		 * An error occurred when reading this battery
		*/
		str->batteries_read_states_curr[which] += 1;

		memcpy(&(str->output_vars_read_curr[which]),
				   &(str->output_vars_read_last[which]),
				   sizeof(Protocol_ReadCmd_OutputVars));

		/*
		 * In case of all failures we need to set manually the id
		 * because the last value might be wrong if no correct read was
		 * ever made
		*/
		str->output_vars_read_curr[which].addr_bank = str->string_id + 1;
		str->output_vars_read_curr[which].addr_batt = which + 1;

	}else{
		str->batteries_read_states_curr[which] = 0;
	}

	/*
	 * At this point we already have the output information, so let's just 
	 * write to the last state
	*/
	memcpy(&(str->output_vars_read_last[which]),
		   &(str->output_vars_read_curr[which]),
		   sizeof(Protocol_ReadCmd_OutputVars));
	

	float fvbat = (float)str->output_vars_read_curr[which].vbat;
	float fitems = (float)str->string_size;
	str->average_vars_curr.average += fvbat / fitems;
	str->average_vars_curr.bus_sum += (unsigned int)str->output_vars_read_curr[which].vbat;
	
	return true;
}


static bool cm_string_read_one_impe(cm_string_t *str, int which, Database_Parameters_t params){
	Protocol_ImpedanceCmd_InputVars	input_impedance = {0};
	int err											= 0;

	input_impedance.addr_bank = str->string_id + 1;
	input_impedance.addr_batt = which + 1;

	err = prot_read_impedance(&input_impedance, 
							  &(str->output_vars_imp_curr[which]),
							  params.param3_messages_wait);
	if(err != 0){
		/*
		 * An error occurred when reading this battery impedance!
		*/
		str->batteries_read_states_curr[which] += 1;

		memcpy(&(str->output_vars_imp_curr[which]),
				   &(str->output_vars_imp_last[which]),
				   sizeof(Protocol_ImpedanceCmd_OutputVars));
	}

	/*
	 * At this point we already have the output information, so let's just 
	 * write to the last state
	*/
	memcpy(&(str->output_vars_imp_last[which]),
		   &(str->output_vars_imp_curr[which]),
		   sizeof(Protocol_ImpedanceCmd_OutputVars));


	return true;
}

static bool cm_string_read_one(cm_string_t *str, const Read_t type,
							   int which, Database_Parameters_t params)
{
	bool ret = false;
	if(which < str->string_size){
		bool read_vars = (type == VARS) || !str->batteries_has_read[which];
		bool read_impe = (type == IMPE) || !str->batteries_has_read[which];
		ret = true;
		if(read_vars){
			ret &= cm_string_read_one_vars(str, which, params);
		}

		/*
		 * We only read impedance if everything so far was ok
		 * no need to increase the failure
		*/
		if(read_impe && ret){
			ret &= cm_string_read_one_impe(str, which, params);
		}

		str->batteries_has_read[which] = true;

	}

	return ret;
}

bool cm_string_do_read_all(cm_string_t *str, const Read_t type,
 						   Database_Parameters_t params, bool firstRead)
{
	PTR_VALID(str);
	if(str->is_inited){
		bool ret = true;
		if(firstRead || type == VARS){
			str->average_vars_curr.average = 0.0f;
			str->average_vars_curr.bus_sum = 0.0f;
		}
		
		for(int j = 0; j < str->string_size; j += 1){

			ret &= cm_string_read_one(str, type, j, params);
			
			if(firstRead){
				ret &= cm_string_read_one(str, !type, j, params);
			}

			/*
			* Pausa entre as leituras das celulas
			*/
			sleep_ms(params.param1_interbat_delay);
		}

		if(firstRead || type == VARS){
			memcpy(&(str->average_vars_last),
			 	   &(str->average_vars_curr),
			 	   sizeof(StringAvg_t));
		}

		return ret;
	}

	return false;
}


static int evaluate_states(cm_string_t *str, int which, Database_Alarmconfig_t *params){
	Protocol_ImpedanceCmd_OutputVars *imp_vars = &(str->output_vars_imp_curr[which]);
	Protocol_ReadCmd_OutputVars 	 *read_vars = &(str->output_vars_read_curr[which]);

	/*
	 * Tensao
	 */
	if (read_vars->vbat < params->tensao_min) {
		str->batteries_states_curr[which].tensao = 1;
	} else if (read_vars->vbat > params->tensao_max) {
		str->batteries_states_curr[which].tensao = 3;
	} else {
		str->batteries_states_curr[which].tensao = 2;
	}

	/*
	 * Temperatura: o valor e com sinal (e possivel ter valores negativos)
	 * Com isso, o cast e necessario, uma vez que o dado recebido e tratado
	 * em numero bruto, sem considerar a sinalizacao (16 bits)
	 */
	if ((int)read_vars->etemp < params->temperatura_min) {
		str->batteries_states_curr[which].temperatura = 1;
	} else if ((int)read_vars->etemp > params->temperatura_max) {
		str->batteries_states_curr[which].temperatura = 3;
	} else {
		str->batteries_states_curr[which].temperatura = 2;
	}

	/*
	 * Impedancia
	 */
	if (imp_vars->impedance < params->impedancia_min) {
		str->batteries_states_curr[which].impedancia = 1;
	} else if (imp_vars->impedance > params->impedancia_max) {
		str->batteries_states_curr[which].impedancia = 3;
	} else {
		str->batteries_states_curr[which].impedancia = 2;
	}

	return 0;
}


static int evaluate_states_results(
		Database_Alarmconfig_t *params,
		unsigned int target,
		unsigned int bus,
		unsigned int disk_capacity,
		Protocol_States *states) 
{

	/*
	 * Tensao de barramento (bus)
	 */
	if (bus < params->barramento_min) {
		states->barramento = 1;
	} else if (bus > params->barramento_max) {
		states->barramento = 3;
	} else {
		states->barramento = 2;
	}

	/*
	 * Tensao Target
	 */
	if (target < params->target_min) {
		states->target = 1;
	} else if (target > params->target_max) {
		states->target = 3;
	} else {
		states->target = 2;
	}

	/*
	 * Capacidade de disco
	 */
	if (disk_capacity > 95) {
		states->disk = 3; 
	} else {
		states->disk = 2;
	}

	return 0;
}

bool cm_string_process_string_alarms(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead)
{
	PTR_VALID(str);

	unsigned int uaverage 				= _compressFloat(str->average_vars_curr.average);
	Protocol_States *pt_state_current 	= &(str->batteries_states_curr[0]);
	Protocol_States *pt_state_last 		= &(str->batteries_states_last[0]);

	str->average_vars_last.average 		= str->average_vars_curr.average;
	str->average_vars_last.bus_sum 		= str->average_vars_curr.bus_sum;

	/*
	 * Processa os estados dos resultados calculados
	 */
	evaluate_states_results(alarmconfig, uaverage, str->average_vars_curr.bus_sum,
							capacity,pt_state_current);

	if (!firstRead) {
		/*
		 * Podem ser gerados ate 3 alarmes por leitura, sendo um para cada leitura critica
		 */
		if(!str->string_ok){
			/* Alarme relacionado a falha em alguma bateria na string */
			int3 codes;
			codes.i1 = str->last_batt_err;
			codes.i2 = str->string_id;
			codes.i3 = str->last_batt_num;
			db_add_alarm(NULL, NULL, NULL, NULL, STRING, codes);
		}
		if (pt_state_current->barramento != pt_state_last->barramento) {
			/* Registra alarme de mudanca de tensao de barramento */
			db_add_alarm_results(str->average_vars_curr.bus_sum,pt_state_current, alarmconfig,BARRAMENTO);
		}
		if (pt_state_current->target != pt_state_last->target) {
			/* Registra alarme de mudanca de tensao target */
			db_add_alarm_results(uaverage,pt_state_current, alarmconfig,TARGET);
		}
		if (pt_state_current->disk != pt_state_last->disk) {
			/* Registra alarme de capacidade de disco */
			db_add_alarm_results(capacity,pt_state_current, alarmconfig,DISK);
		}
	}

	/*
	 * Atualiza os valores antigos
	 */
	pt_state_last->barramento = pt_state_current->barramento;
	pt_state_last->target = pt_state_current->target;
	pt_state_last->disk = pt_state_current->disk;

	return true;

}

bool cm_string_process_batteries(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead)
{
	PTR_VALID(str);
	int err = 0;
	Protocol_States 					*pt_state_current;
	Protocol_States 					*pt_state_last;
	Protocol_ReadCmd_OutputVars			*pt_vars;
	Protocol_ImpedanceCmd_OutputVars 	*pt_imp;
	unsigned int uaverage = _compressFloat(str->average_vars_curr.average);
	int3 dummy = {0};
	str->string_ok = true;
	for(int j = 0; j < str->string_size; j += 1){
		pt_vars 			= &(str->output_vars_read_last[j]);
		pt_state_current 	= &(str->batteries_states_curr[j]);
		pt_state_last 		= &(str->batteries_states_last[j]);
		pt_imp 				= &(str->output_vars_imp_last[j]);

		evaluate_states(str, j, alarmconfig);
		
		int state = str->batteries_read_states_curr[j] > params.param3_messages_wait ? 1 : 0;
		if(state != 0 && str->string_ok){
			str->last_batt_err = -3; //timeout is the only logical error
			str->last_batt_num = j;
			str->string_ok = false;
		}

		err = db_add_response(pt_vars, pt_imp, pt_state_current, j+1, save_log_state, state, uaverage);

		if(!firstRead){
			/*
			 * Podem ser gerados ate 3 alarmes por leitura, sendo um para cada leitura critica
			 */
			if (pt_state_current->tensao != pt_state_last->tensao) {
				/* Registra alarme de mudanca de tensao */
				db_add_alarm(pt_vars,pt_imp,pt_state_current,alarmconfig,TENSAO, dummy);
			}
			if (pt_state_current->temperatura != pt_state_last->temperatura) {
				/* Registra alarme de mudanca de temperatura */
				db_add_alarm(pt_vars,pt_imp,pt_state_current,alarmconfig,TEMPERATURA, dummy);
			}
			if (pt_state_current->impedancia != pt_state_last->impedancia) {
				/* Registra alarme de mudanca de impedancia */
				db_add_alarm(pt_vars,pt_imp,pt_state_current,alarmconfig,IMPEDANCIA, dummy);
			}
		}

		/*
		 * Atualiza os valores antigos
		 */
		pt_state_last->tensao = pt_state_current->tensao;
		pt_state_last->temperatura = pt_state_current->temperatura;
		pt_state_last->impedancia = pt_state_current->impedancia;
	}

	db_update_average(uaverage, str->average_vars_curr.bus_sum, str->string_id);

	return true;
}

/*
 * Clear all buffers contained in this CM-String
*/

bool cm_string_destroy(cm_string_t **str){
	PTR_VALID(str);
	PTR_VALID(*str);

	if((*str)->is_inited){
		free((*str)->output_vars_read_last);
		free((*str)->output_vars_read_curr);
		free((*str)->output_vars_imp_last);
		free((*str)->output_vars_imp_curr);
		free((*str)->batteries_read_states_last);
		free((*str)->batteries_read_states_curr);
		free((*str)->batteries_states_curr);
		free((*str)->batteries_states_last);
		free((*str)->batteries_has_read);
	}

	free((*str));
	(*str) = nullptr;
	return true;
}