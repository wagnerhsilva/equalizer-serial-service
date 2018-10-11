#include <component.h>
#include <time.h>

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

/* Flavio Alves
 * Inclusao de variavel inicial para modo de teste de tabela de tendencias
 */
static struct timeval InitialTime;

/*
 * Basic interface to handle CM-Strings operations
*/

/*
 * Creates a new CM-String received as an address to a pointer to cm_string_t
 * with the desired size 'size'
*/
bool cm_string_new(cm_string_t **str, int size){
	PTR_VALID(str);
	/*
	 * If the pointer is not null than this cm_string_t is already
	 * inited and we do not touch it
	*/
	if(*str != NULL){
		printf("Tried to init already inited pointer!\n");
		return false;
	}
	/*
	 * Allocates memory for this CM-String components using the PTR_ATT define.
	 * PTR_ATT(x, y) makes a pointer attribution, it gives *x the value y and checks
	 * if the operation succeed. In case it does it does nothing, in case of failure
	 * it logs a message and returns false.
	*/
	PTR_ATT(&(*str), (cm_string_t *)malloc(sizeof(cm_string_t)));
	(*str)->is_inited = false;
	(*str)->string_size = -1;
	/*
	 * Clear the bitmask, assume each battery is ok
	*/
	bits_create_new(&((*str)->battery_mask));
	if(size > 0){ //if size is actually positive, allocate its content
		(*str)->string_size = size;
		PTR_ATT(&((*str)->output_vars_read_last), 
			(Protocol_ReadCmd_OutputVars *)calloc(size, sizeof(Protocol_ReadCmd_OutputVars)));
		PTR_ATT(&((*str)->output_vars_read_curr),
			(Protocol_ReadCmd_OutputVars *)calloc(size, sizeof(Protocol_ReadCmd_OutputVars)));
		
		PTR_ATT(&((*str)->output_vars_imp_last), 
			(Protocol_ImpedanceCmd_OutputVars *)calloc(size, sizeof(Protocol_ImpedanceCmd_OutputVars)));
		PTR_ATT(&((*str)->output_vars_imp_curr),
			(Protocol_ImpedanceCmd_OutputVars *)calloc(size, sizeof(Protocol_ImpedanceCmd_OutputVars)));
		
		PTR_ATT(&((*str)->batteries_read_states_curr),
			(int *)calloc(size, sizeof(int)));
		
		PTR_ATT(&((*str)->batteries_states_curr),
			(Protocol_States *)calloc(size, sizeof(Protocol_States)));
		PTR_ATT(&((*str)->batteries_states_last),
			(Protocol_States *)calloc(size, sizeof(Protocol_States)));

		PTR_ATT(&((*str)->batteries_has_read),
			(int *)calloc(size, sizeof(int)));

		(*str)->string_ok = true;
		(*str)->is_inited = true;

	}
	return true;
}

/*
 * Sets the desired id for this CM-String, the id given
 * should match the id of the representation of this CM-String. 
 * Example: an id with value 0 represents the first bank of batteries
 * S1. We have than that:
 * 	id = 0   =>   S1
 * 	id = 1   =>   S2
 *  id = 2   =>   S3
 * ...
*/
bool cm_string_set_id(cm_string_t **str, int id){
	PTR_VALID(str);
	PTR_VALID(*str);
	(*str)->string_id = id;
	return true;
}

/*
 * Performs a serial read of the variables of a single battery given by 'which' 
 * using params to controll timeout and flags. 
*/
static bool cm_string_read_one_vars(cm_string_t *str, int which, Database_Parameters_t params){
	Protocol_ReadCmd_InputVars 	 input_vars 	= {0};
	int err										= 0;
	/*
	 * Set information about which battery we wish to read
	*/
	input_vars.addr_bank = str->string_id + 1;
	input_vars.addr_batt = which + 1;

	/*
	 * Set the average value for vref and configuration
	 * for duty values
	*/
	input_vars.vref = str->average_vars_last.average;
	input_vars.duty_min = params.duty_min;
	input_vars.duty_max = params.duty_max;

	/*
	 * If the previous read for this string showed a problem
	 * we need to turn off equalization. This is done by
	 * settings the duty_max variable to zero.
	*/
	if(!str->string_ok){
		input_vars.duty_max = 0;
	}

	/*
	 * Actually reads the variables from serial by relying on the
	 * protocol API
	*/
	input_vars.index = params.index;
	err = prot_read_vars(&input_vars,
	 					 &(str->output_vars_read_curr[which]),
	 					 params.param3_messages_wait);
	if(err != 0){
		/*
		 * An error occurred when reading this battery
		 * Increment only if it is not currently triggering the alarm
		 * this prevents this int value to grow too much and possibly break
		*/
		if(str->batteries_read_states_curr[which] < params.param3_messages_wait){
			str->batteries_read_states_curr[which] += 1;
		}
		/* Copy last values for this read */
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

		LOG("Component:Timeout on M%d, error count: %d\n", which+1, str->batteries_read_states_curr[which]);

	}else{
		/* In case of no errors make sure that if we hade previous errors they no longer are registered */
		str->batteries_read_states_curr[which] = 0;
	}

	/*
	 * At this point we already have the output information, so let's just 
	 * write to the last state
	*/
	memcpy(&(str->output_vars_read_last[which]),
		   &(str->output_vars_read_curr[which]),
		   sizeof(Protocol_ReadCmd_OutputVars));
	
	/* Compute the contribution of this battery to average values for this CM-String */
	float fvbat = (float)str->output_vars_read_curr[which].vbat;
	float fitems = (float)str->string_size;
	str->average_vars_curr.average += fvbat / fitems;
	str->average_vars_curr.bus_sum += (unsigned int)str->output_vars_read_curr[which].vbat;
	
	return true;
}

/*
 * Performs a impedance read of a single battery given by 'which' using 'params' to control
 * flags and timeouts
*/
static bool cm_string_read_one_impe(cm_string_t *str, int which, Database_Parameters_t params){
	Protocol_ImpedanceCmd_InputVars	input_impedance = {0};
	int err											= 0;
	/*
	 * Set information about which battery we wish to read
	*/
	input_impedance.addr_bank = str->string_id + 1;
	input_impedance.addr_batt = which + 1;

	/*
	 * Read the impedance relying on the protocol API
	*/
	err = prot_read_impedance(&input_impedance, 
							  &(str->output_vars_imp_curr[which]),
							  params.param3_messages_wait);
	if(err != 0){
		/*
		 * An error occurred when reading this battery
		 * Increment only if it is not currently triggering the alarm
		 * this prevents this int value to grow too much and possibly break
		*/
		if(str->batteries_read_states_curr[which] < params.param3_messages_wait){
			str->batteries_read_states_curr[which] += 1;
		}
		/* Copy last values for this read */
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

/*
 * Reads a single battery given by 'which' with a generic read type 'type'
 * and control parameters 'params'
*/
static bool cm_string_read_one(cm_string_t *str, const Read_t type,
							   int which, Database_Parameters_t params)
{
	bool ret = false;
	if(which < str->string_size){ //check if inside our CM-String size
		/* Check if we need to read both values or a single one */
		bool read_vars = (type == VARS) || !str->batteries_has_read[which];
		bool read_impe = (type == IMPE) || !str->batteries_has_read[which];
		ret = true;
		/* read variables for this battery */
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
		/* set this battery as read */
		str->batteries_has_read[which] = true;

	}

	return ret;
}

/*
 * Performs a read with type 'type' on all batteries present on the CM-String 'str'.
 * Use flags to control parameters 'params' 
*/
bool cm_string_do_read_all(cm_string_t *str, const Read_t type,
 						   Database_Parameters_t params, bool firstRead)
{
	PTR_VALID(str);
	if(str->is_inited){ //only read if the pointers have been initalized
		bool ret = true;
		/* in case we are going to read variables we need to reset the current average value */
		if(firstRead || type == VARS){
			str->average_vars_curr.average = 0.0f;
			str->average_vars_curr.bus_sum = 0.0f;
		}
		/* for each battery in this CM-String read 'type' */
		for(int j = 0; j < str->string_size; j += 1){
			ret &= cm_string_read_one(str, type, j, params);
			/*
			* Pause the read beetween batteries
			*/
			sleep_ms(params.param1_interbat_delay);
		}

		/* copy results of average values */
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

	/* Flavio Alves: Ticket #5829
	 * Revisao completa da organizacao dos estados
	 * das leituras de tensao, temperatura e impedancia.
	 * Deve-se seguir a mesma regra usada para as cores
	 * amarela, verde e vermelha presente na interface web
	 * No caso:
	 * - Valor 1 (Amarelo): pre alarme
	 * - Valor 2 (Verde): normal
	 * - Valor 3 (Vermelho) : alarme
	 */

	/*
	 * Tensao
	 * Flavio Alves: Ticket #5829
	 */
	if ((read_vars->vbat <= params->tensao_min) ||
	    (read_vars->vbat >= params->tensao_max)) {
		str->batteries_states_curr[which].tensao = 3;
	} else if ((read_vars->vbat > params->tensao_premin) &&
	           (read_vars->vbat < params->tensao_premax)) {
		str->batteries_states_curr[which].tensao = 2;
	} else {
		str->batteries_states_curr[which].tensao = 1;
	}

	/*
	 * Temperatura: o valor e com sinal (e possivel ter valores negativos)
	 * Com isso, o cast e necessario, uma vez que o dado recebido e tratado
	 * em numero bruto, sem considerar a sinalizacao (16 bits)
	 * Flavio Alves: Ticket #5829
	 */
	if (((int)read_vars->etemp <= params->temperatura_min) ||
	    ((int)read_vars->etemp >= params->temperatura_max)) {
		str->batteries_states_curr[which].temperatura = 3;
	} else if (((int)read_vars->etemp > params->temperatura_premin) &&
	           ((int)read_vars->etemp < params->temperatura_premax)) {
		str->batteries_states_curr[which].temperatura = 2;
	} else {
		str->batteries_states_curr[which].temperatura = 1;
	}

	/*
	 * Impedancia
	 * Flavio Alves: Ticket #5829
	 */
	if ((imp_vars->impedance <= params->impedancia_min) ||
	    (imp_vars->impedance >= params->impedancia_max)) {
		str->batteries_states_curr[which].impedancia = 3;
	} else if ((imp_vars->impedance > params->impedancia_premin) && 
	           (imp_vars->impedance < params->impedancia_premax)) {
		str->batteries_states_curr[which].impedancia = 2;
	} else {
		str->batteries_states_curr[which].impedancia = 1;
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

/*
 * Process the CM-String 'str' in search of 'String'-like alarms
*/
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
			for(int i = 0; i < str->string_size; i += 1){
				if(bits_is_bit_set(&(str->battery_mask), i)){
					int3 codes;
					codes.i1 = -3; //timeout is the only logical error
					codes.i2 = str->string_id;
					codes.i3 = i;
					db_add_alarm(NULL, NULL, NULL, NULL, STRING, codes);

					/* Clear the bit so that when we start over the mask is clear
					 * Let's make use of this loop to clear this position since we already 
					 * triggered the alarm
					*/
					bits_set_bit(&(str->battery_mask), i, false);
				}
			}
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

static bool cm_string_handle_tendence(Protocol_ReadCmd_OutputVars * OutVars, 
							   	      Protocol_ImpedanceCmd_OutputVars * OutImpe,
									  Protocol_States *States,
									  time_t CurrentTime, Tendence_Configs_t *Configs)
{
	Tendence_t tendence = {0};
	tendence.MesaureInteraction = Configs->LastIteration + 1;
	tendence.Battery = OutVars->addr_batt;
	tendence.String = OutVars->addr_bank;
	tendence.CurrentTime = CurrentTime;
	tendence.Temperature = OutVars->etemp;
	tendence.Impendance = OutImpe->impedance;
	LOG("Component::Writing tendence properties for [%d x %d]\n",
	 	tendence.String, tendence.Battery);	
	return (db_add_tendence(tendence) == 0);
}

/* Flavio Alves
 * Usando um contador para fazer a pausa para o modo teste, para criação
 * da tabela de tendencias
 */
#define TESTMODE_MAX_WAIT_TIME 12
static int testModeCounter = 0;

/*
 * Process all batteries for battery-like alarms
*/
bool cm_string_process_batteries(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int save_log_state,
							   bool firstRead, int was_global_read_ok)
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
	
	bool TendencesWrote = true;
	int WriteTendences = 0;
	double Months = 0;
	int PreviousWrite = 0;
	int TendencePeriod = 0;
	time_t CurrentTime;
	if(TendenceOpts.IsConfigured == 1){
		/* Flavio Alves
		 * Incluindo um modo de teste, onde o dado é escrito na tabela a cada 10 minutos
		 */
		if (TendenceOpts.testMode) {
			WriteTendences = 0;
			if (testModeCounter < TESTMODE_MAX_WAIT_TIME) {
				testModeCounter++;
			} else {
				LOG("TESTMODE: time elapsed\n");
				if (was_global_read_ok > 0) {
					LOG("WriteTendences\n");
					WriteTendences = 1;
				}
				/* Reseta o contador */
				testModeCounter = 0;
			}
		} else {
			PreviousWrite = TendenceOpts.HasWrites;
			CurrentTime = GetCurrentTime();
			TendencePeriod = (TendenceOpts.HasWrites == 1 ?
					TendenceOpts.PeriodConstant :
					TendenceOpts.PeriodInitial);
			char month0[80], month1[80];
			GetTimeString(month0, 80, "%d/%m/%Y", CurrentTime);
			GetTimeString(month1, 80, "%d/%m/%Y", TendenceOpts.LastWrite);
			Months = GetDifferenceInMonths(CurrentTime, TendenceOpts.LastWrite);
			LOG("Months [%s - %s] : %g => Used period: %d\n", month0, month1, Months, TendencePeriod);
			/*
			 * Only writes when the date period is reached AND we have a full read
			 * otherwise we could get critical errors since if one battery fails
			 * it will need to wait (several) months before the next read
			 */
			WriteTendences = (Months >= TendencePeriod && was_global_read_ok > 0 ? 1 : 0);
		}
	}

	for(int j = 0; j < str->string_size; j += 1){
		int global_id 		= str->string_id * str->string_size + j + 1;
		pt_vars 			= &(str->output_vars_read_last[j]);
		pt_state_current 	= &(str->batteries_states_curr[j]);
		pt_state_last 		= &(str->batteries_states_last[j]);
		pt_imp 				= &(str->output_vars_imp_last[j]);

		evaluate_states(str, j, alarmconfig);
		
		int state = str->batteries_read_states_curr[j] >= params.param3_messages_wait ? 1 : 0;
		if(state != 0){
			str->string_ok = false;
			bits_set_bit(&(str->battery_mask), j, true);
		}

		err = db_add_response(pt_vars, pt_imp, pt_state_current,
		 				global_id, save_log_state, state, uaverage);

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

	if(str->string_ok){
		for(int j = 0; j < str->string_size; j += 1){
			int global_id 		= str->string_id * str->string_size + j + 1;
			pt_vars 			= &(str->output_vars_read_last[j]);
			pt_state_current 	= &(str->batteries_states_curr[j]);
			pt_state_last 		= &(str->batteries_states_last[j]);
			pt_imp 				= &(str->output_vars_imp_last[j]);

			if(WriteTendences == 1){
				TendencesWrote &= cm_string_handle_tendence(pt_vars, pt_imp, pt_state_current,
								 							CurrentTime, &TendenceOpts);
			}
		}
	}
	
	db_update_average(uaverage, str->average_vars_curr.bus_sum, str->string_id);

	if(WriteTendences == 1){
		if (TendencesWrote){
			TendenceOpts.HasWrites = 1;
			TendenceOpts.LastWrite = GetCurrentTime();
			TendenceOpts.LastIteration += 1;
			db_update_tendence_configs(TendenceOpts);
		}
		else{
			/*
			 * TODO: Recover?
			*/
		}
	}else{
		LOG("Component::Not updating tendencies\n");
	}

	return true;
}

/*
 * Returns id for the CM-String 'str'
*/
int cm_string_get_id(cm_string_t *str){
	PTR_VALID(str);
	return str->string_id;
}

/*
 * Clear all buffers contained in the CM-String '*str'
*/

bool cm_string_destroy(cm_string_t **str){
	PTR_VALID(str);
	PTR_VALID(*str);

	if((*str)->is_inited){
		free((*str)->output_vars_read_last);
		free((*str)->output_vars_read_curr);
		free((*str)->output_vars_imp_last);
		free((*str)->output_vars_imp_curr);
		free((*str)->batteries_read_states_curr);
		free((*str)->batteries_states_curr);
		free((*str)->batteries_states_last);
		free((*str)->batteries_has_read);
	}

	free((*str));
	(*str) = nullptr;
	return true;
}
