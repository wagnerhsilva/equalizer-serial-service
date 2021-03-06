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
* Inclusao de variaveis para leitura do modulo sensor de corrente
*/
unsigned short 			current[255];
char 					orientation[255];
unsigned int 			currentSensorCheckbox;

//Status do Módulo - inclusão posterior seguindo o padrão para informar na tabela Medias
unsigned short			status_current[255]; //recebe geral_status_current para cada string e informa ao BD
unsigned short			geral_status_current[255]; //variável que recebe valor de alarme e erro no dado de orientation (0->ok 1->falha) (ESCRITA EM VÁRIOS PONTOS)
unsigned short			previous_current[255] = {0}; //último valor lido de corrente, quando comunicação ok dos módulos para incluir na tabela Medias, apenas
unsigned short			alarm_curr[255]={0}; //seta variável para indicar que os limites foram ultrapassados 

unsigned int 			add_alerta_curr[255]={0}; //verifica a falta de comunicação do módulo e do sensor de corrente (orientation == 2 e == 3)
/*
Inclusão de variável para receber valor de target e calcular lógica
 */
float target_num_bat=0; //conta número de baterias dentro da faixa de 0,5% do target
int controle_equalizacao_ativado=0; //mostra se o controle de duty_cicle foi ativado em algum momento
float valor_average_target=0; //valor de target a ser recebido quando a string completa for somada à variável str
int permite_exec_rotina=0; //ativação do controle
float valor_tensao_bat[256]; //vetor que recebe string e verifica valor de cada bateria

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

	//Controla equalização limitando o valor máximo para a leitura atual da string selecionada
	if(controle_equalizacao_ativado==1){
		LOG("LINHA 140, CONTROLE DE EQUALIZACAO ATIVADO");
		params.duty_max = 9000;
		
	}else{
		LOG("LINHA 144, CONTROLE DE EQUALIZACAO DESATIVADO");
		params.duty_max = 45000;
	}

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

	//recebe o valor de tensão de cada bateria da string
	valor_tensao_bat[which] = fvbat;

	//verifica se existem mais de duas baterias e faz compensação de float no número de baterias para, de fato, executar a rotina de controle de equalização
	if((int)fitems > 2){
		if(((float)which==(fitems-1)) || ((float)which==fitems)){
			permite_exec_rotina = 1;
			valor_average_target = str->average_vars_curr.average;
			LOG("VERIFICACAO DE NUM DE BAT NA STRING");
		}
	}

	//execução de rotina de controle de equalização para permitir ou não o controle de equalização a partir da próxima leitura de string
	if(permite_exec_rotina==1){

		LOG("PERMITE EXEC DE ROTINA DE CONTROLE");
		//verificação de tensão de cada bateria, se todas estiverem dentro de +ou- 0,65% do target, o controle poderá ser ativado
		for(int a = 0; a <= which; a++){
			if((valor_tensao_bat[a]>=(0.9965*valor_average_target)) && (valor_tensao_bat[a]<=(1.0035*valor_average_target))){
				target_num_bat++;				
			}
		}

		//verificação de qtd de baterias dentro da faixa estabelecida para controle
		if(target_num_bat>=(fitems)){
			LOG("BATERIAS DENTRO DE FAIXA PARA CONTROLE ----> ATIVADO");
			controle_equalizacao_ativado = 1;
		}else if(target_num_bat<=(fitems-3)){//número de baterias limite que pode estar fora do target para desligar o controle
			LOG("BATERIAS FORA DE FAIXA PARA CONTROLE -----> DESATIVADO");
			controle_equalizacao_ativado = 0;
		}

		//reinicia variáveis para leituras posteriores da string
		permite_exec_rotina = 0;
		target_num_bat = 0;

		LOG("SAI DA ROTINA");
	}

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
	int errCurrentSensor=0;
	Protocol_ReadCmd_InputVars readCurrentInputVars =  {0};
	Protocol_ReadCmd_OutputVars readCurrentOutputVars = {0};

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

		/*
		* Inclusão de Leitura do sensor de corrente caso habilitada
		*/
		if (currentSensorCheckbox)
		{
			LOG("STRING %d - LEITURA DO SENSOR DE CORRENTE HABILITADA\n", (str->string_id+1));
			//LOG("ATRIBUICAO: %u\n", currentSensorCheckbox);
			readCurrentInputVars.addr_bank = str->string_id + 1;
			readCurrentInputVars.addr_batt = str->string_size + 1;
			readCurrentInputVars.vref = str->average_vars_last.average;
			readCurrentInputVars.duty_min = params.duty_min;
			readCurrentInputVars.duty_max = params.duty_max;
			readCurrentInputVars.index = params.index;

			errCurrentSensor = prot_read_vars(&readCurrentInputVars, &readCurrentOutputVars, params.param3_messages_wait);

			if(errCurrentSensor == 0)
			{
				LOG("STRING %d - CORRENTE = %u\n", (str->string_id+1), current[str->string_id]);
				LOG("STRING %d - SENTIDO DA CORRENTE = %u\n", (str->string_id+1), orientation[str->string_id]);
				//LOG("ATRIBUICAO: %u\n", currentSensorCheckbox);
				current[str->string_id] = readCurrentOutputVars.currentRead;
				orientation[str->string_id] = readCurrentOutputVars.currentOrientation;
				geral_status_current[str->string_id] = 0;
				add_alerta_curr[str->string_id] = 0;
			}
			else
			{
				LOG("STRING %d - ERRO NA COMUNICACAO COM O SENSOR DE CORRENTE\n", (str->string_id+1));
				LOG("STRING %d - CORRENTE = %u\n", (str->string_id+1), current[str->string_id]);
				LOG("STRING %d - SENTIDO DA CORRENTE = %u\n", (str->string_id+1), orientation[str->string_id]);
				//LOG("ATRIBUICAO: %u\n", currentSensorCheckbox);
				current[str->string_id] = 0;
				orientation[str->string_id] = 3;
				geral_status_current[str->string_id] = 1;
				add_alerta_curr[str->string_id] = 2;
			}

			sleep_ms(params.param1_interbat_delay);
		}
		else
		{
			LOG("STRING %d - LEITURA DO SENSOR DE CORRENTE DESABILITADA\n", (str->string_id+1));
			LOG("STRING %d - CORRENTE = %u\n", (str->string_id+1), current[str->string_id]);
			LOG("STRING %d - SENTIDO DA CORRENTE = %u\n", (str->string_id+1), orientation[str->string_id]);
			//LOG("ATRIBUICAO: %u\n", currentSensorCheckbox);
			current[str->string_id] = 0;
			orientation[str->string_id] = 4;
			geral_status_current[str->string_id] = 1;
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

	extern Database_SharedMem_t *shared_mem_ptr;
	/*
	 * Revisao dos estados de alarmes:
	 * 1 - minimo detectado
	 * 2 - pre-minimo detectado
	 * 3 - normal
	 * 4 - pre-maximo detectado
	 * 5 - maximo detectado
	 */
	// LOG("Evaluating states ... \n");
	// LOG("     TENSAO:%d\tMIN:%d\tPREMIN:%d\tPREMAX:%d\tMAX:%d\n",
	// 		read_vars->vbat,params->tensao_min,params->tensao_premin,
	// 		params->tensao_premax,params->tensao_max);
	// LOG("TEMPERATURA:%d\tMIN:%d\tPREMIN:%d\tPREMAX:%d\tMAX:%d\n",
	// 		read_vars->etemp,params->temperatura_min,params->temperatura_premin,
	// 		params->temperatura_premax,params->temperatura_max);
	// LOG(" IMPEDANCIA:%d\tMIN:%d\tPREMIN:%d\tPREMAX:%d\tMAX:%d\n",
	// 		imp_vars->impedance,params->impedancia_min,params->impedancia_premin,
	// 		params->impedancia_premax,params->impedancia_max);
	/*
	 * TENSAO
	 */
	if (read_vars->vbat <= params->tensao_min) {
		/* Minimo */
		str->batteries_states_curr[which].tensao = 1;
	} else {
		if (read_vars->vbat <= params->tensao_premin) {
			/* Pre-Minimo */
			str->batteries_states_curr[which].tensao = 2;
		} else {
			if (read_vars->vbat < params->tensao_premax) {
				/* Normal */
				str->batteries_states_curr[which].tensao = 3;
			} else {
				if (read_vars->vbat <= params->tensao_max) {
					/* Pre-Maximo */
					str->batteries_states_curr[which].tensao = 4;
				} else {
					/* Maximo */
					str->batteries_states_curr[which].tensao = 5;
				}
			}
		}
	}

	// LOG("str->batteries_states_curr[%d].tensao=%d\n",which,
	// 	str->batteries_states_curr[which].tensao);

	/*
	 * TEMPERATURA
	 */
	if (read_vars->etemp <= params->temperatura_min) {
		/* Minimo */
		str->batteries_states_curr[which].temperatura = 1;
	} else {
		if (read_vars->etemp <= params->temperatura_premin) {
			/* Pre-Minimo */
			str->batteries_states_curr[which].temperatura = 2;
		} else {
			if (read_vars->etemp < params->temperatura_premax) {
				/* Normal */
				str->batteries_states_curr[which].temperatura = 3;
			} else {
				if (read_vars->etemp <= params->temperatura_max) {
					/* Pre-Maximo */
					str->batteries_states_curr[which].temperatura = 4;
				} else {
					/* Maximo */
					str->batteries_states_curr[which].temperatura = 5;
				}
			}
		}
	}

	// LOG("str->batteries_states_curr[%d].temperatura=%d\n",which,
	// 	str->batteries_states_curr[which].temperatura);

	/*
	 * IMPEDANCIA
	 */
	if (imp_vars->impedance <= params->impedancia_min) {
		/* Minimo */
		str->batteries_states_curr[which].impedancia = 1;
	} else {
		if (imp_vars->impedance <= params->impedancia_premin) {
			/* Pre-Minimo */
			str->batteries_states_curr[which].impedancia = 2;
		} else {
			if (imp_vars->impedance < params->impedancia_premax) {
				/* Normal */
				str->batteries_states_curr[which].impedancia = 3;
			} else {
				if (imp_vars->impedance <= params->impedancia_max) {
					/* Pre-Maximo */
					str->batteries_states_curr[which].impedancia = 4;
				} else {
					/* Maximo */
					str->batteries_states_curr[which].impedancia = 5;
				}
			}
		}
	}

	// LOG("str->batteries_states_curr[%d].impedancia=%d\n",which,
	// 	str->batteries_states_curr[which].impedancia);

	/*
	 * Atualiza a memoria compartilhada (modbus)
	 */
	// LOG("Updating battery states: %d %d %d: %d %d %d\n",
	// 		str->string_id, str->string_size, which,
	// 		str->batteries_states_curr[which].tensao,
	// 		str->batteries_states_curr[which].temperatura,
	// 		str->batteries_states_curr[which].impedancia);
	shared_mem_ptr->bat_alarms[str->string_id*str->string_size+which].tensao = str->batteries_states_curr[which].tensao;
	shared_mem_ptr->bat_alarms[str->string_id*str->string_size+which].temperatura = str->batteries_states_curr[which].temperatura;
	shared_mem_ptr->bat_alarms[str->string_id*str->string_size+which].impedancia = str->batteries_states_curr[which].impedancia;

	return 0;
}


static int evaluate_states_results(
		Database_Alarmconfig_t *params,
		unsigned int target,
		unsigned int bus,
		unsigned int disk_capacity,
		unsigned int current, 
		unsigned int str_id,
		Protocol_States *states) 
{
	extern Database_SharedMem_t *shared_mem_ptr;

	/*
	 * Corrente
	 */
	if (params->corrente_min < current && orientation[str_id]==1) {
		states->corrente = 1;
		alarm_curr[str_id]=1;	
		if(add_alerta_curr[str_id]==1){
			states->corrente = 4;
		}else if(add_alerta_curr[str_id]==2){
			states->corrente = 5;
		}
	} else if (current > params->corrente_max && orientation[str_id]==0) {
		states->corrente = 3;
		alarm_curr[str_id]=3;
		if(add_alerta_curr[str_id]==1){
			states->corrente = 4;
		}else if(add_alerta_curr[str_id]==2){
			states->corrente = 5;
		}
	} else {
		states->corrente = 2;
		alarm_curr[str_id]=2;
		if(add_alerta_curr[str_id]==1){
			states->corrente = 4;
		}else if(add_alerta_curr[str_id]==2){
			states->corrente = 5;
		}
	}

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
	 * (Flavio Alves: o estado "abaixo de 95%" praticamente
	 * nao e mais necessario. Porem foi mantido para fins de
	 * compatibilidade)
	 */
	if (disk_capacity > 90) {
		if (disk_capacity > 95) {
			states->disk = 3; 
		} else {
			states->disk = 5;
		}
	} else {
		states->disk = 4;
	}
	
	/*
	 * Atualiza a memoria compartilhada (modbus)
	 */
	// LOG("Updating results: %d %d %d\n",
	// 		states->barramento,
	// 		states->target,
	// 		states->disk);
	shared_mem_ptr->barramento = states->barramento;
	shared_mem_ptr->target = states->target;
	shared_mem_ptr->disco = states->disk;
	shared_mem_ptr->corrente = states->corrente;

	return 0;
}

/*
 * Process the CM-String 'str' in search of 'String'-like alarms
*/
bool cm_string_process_string_alarms(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
							   Database_Parameters_t params, int capacity, bool firstRead)
{
	/* Flavio Alves: ticket #5821
	 * Inclusao de um mecanismo simples para detectar se foi enviado email de alarme
	 * indicando problema de comunicacao na serial.
	 */
	static int serialProblemSent = 0;
	static int serialProblemMorningSent = 0;
	int3 codes;
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
							capacity, current[str->string_id], str->string_id, pt_state_current);

	if (!firstRead) {
		/*
		 * Varredura das flags
		 */
		for(int i = 0; i < str->string_size; i += 1) {
			// LOG("Flavio Alves: string[%d]: %d: alarm_state=%d: msg_sent=%d\n",
			// 		i, str->string_ok, str->battery_mask.alarm_state[i], serialProblemSent);
			/* Problemas com serial */
			if (!str->string_ok) {
				codes.i1 = -3; //timeout is the only logical error
				codes.i2 = str->string_id;
				codes.i3 = i;
				if(bits_is_bit_set(&(str->battery_mask), i)) {	
					if (!serialProblemSent) {
						/* Flavio Alves: ticket #5821
						* Envia somente uma mensagem de problema de serial,
						* mesmo que o problema ocorra com varias strings
						*/
						db_add_alarm_timeout(&(str->battery_mask),codes);
						serialProblemSent = 1;
					} else {
						/* Caso a bateria esteja com problemas, a notificacao ja tiver
						* sido enviada e for 8h00, o estado de envio do alarme e resetado
						* para que uma nova mensagem possa ser enviada.
						*/
						time_t rawtime;   
						time ( &rawtime );
						struct tm *timeinfo = localtime (&rawtime);
						if ((timeinfo->tm_hour == 8) && (timeinfo->tm_min == 0)) {
							if (serialProblemMorningSent == 0) {
								LOG("Dia seguinte de persistencia do problema\n");
								db_add_alarm_timeout(&(str->battery_mask),codes);
								serialProblemMorningSent = 1;
							}
						} else {
							if (serialProblemMorningSent) {
								serialProblemMorningSent = 0;
							}
						}
					}
					/* Muda o estado para alarme enviado, mesmo que a 
					 * mensagem especifica de alarme nao corresponda
					 * a string em questao */
					str->battery_mask.alarm_state[i] = 2;
					
					/* Clear the bit so that when we start over the mask is clear
					 * Let's make use of this loop to clear this position since we already 
					 * triggered the alarm
					*/
					bits_set_bit(&(str->battery_mask), i, false);
				}
			} else {
				if (str->battery_mask.alarm_state[i]) {
					if (serialProblemSent) {
						codes.i1 = -3; //timeout is the only logical error
						codes.i2 = str->string_id;
						codes.i3 = i;
						/* Muda o estado para configurar o alarme a ser enviado */
						str->battery_mask.alarm_state[i] = 3;
						db_add_alarm_timeout(&(str->battery_mask),codes);
						serialProblemSent = 0;
					}
					/* Vai para o estado de idle */
					str->battery_mask.alarm_state[i] = 0;
				}
			}
		}

		// /*
		//  * Podem ser gerados ate 3 alarmes por leitura, sendo um para cada leitura critica
		//  */
		// if(!str->string_ok){
		// 	/* Flavio Alves: ticket #5821
		// 	 * inicializa identificador de problemas a cada iteracao.
		// 	 */
		// 	// LOG("Flavio Alves: problemas com a string\n");
		// 	/* Alarme relacionado a falha em alguma bateria na string */
		// 	for(int i = 0; i < str->string_size; i += 1){
				
		// 		}
		// 	}
		// }


		if (pt_state_current->barramento != pt_state_last->barramento) {
			/* Registra alarme de mudanca de tensao de barramento */
			db_add_alarm_results(str->average_vars_curr.bus_sum,pt_state_current, alarmconfig,BARRAMENTO, str->string_id);
		}
		if (pt_state_current->target != pt_state_last->target) {
			/* Registra alarme de mudanca de tensao target */
			db_add_alarm_results(uaverage,pt_state_current, alarmconfig,TARGET, str->string_id);
		}
		if (pt_state_current->disk != pt_state_last->disk) {
			/* Registra alarme de capacidade de disco */
			db_add_alarm_results(capacity,pt_state_current, alarmconfig,DISK, str->string_id);
		}
		if (pt_state_current->corrente != pt_state_last->corrente){
			/* Registra alarme de corrente da string */
			db_add_alarm_results(current[str->string_id],pt_state_current,alarmconfig, CORRENTE, str->string_id);
		}
	}

	/*
	 * Atualiza os valores antigos
	 */
	pt_state_last->barramento = pt_state_current->barramento;
	pt_state_last->target = pt_state_current->target;
	pt_state_last->disk = pt_state_current->disk;
	pt_state_last->corrente = pt_state_current->corrente;

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
	
	// bool TendencesWrote = true;
	// int WriteTendences = 0;
	// double Months = 0;
	// int PreviousWrite = 0;
	// int TendencePeriod = 0;
	// time_t CurrentTime;
	// if(TendenceOpts.IsConfigured == 1){
	// 	/* Flavio Alves
	// 	 * Incluindo um modo de teste, onde o dado é escrito na tabela a cada 10 minutos
	// 	 */
	// 	if (TendenceOpts.testMode) {
	// 		WriteTendences = 0;
	// 		if (testModeCounter < TESTMODE_MAX_WAIT_TIME) {
	// 			testModeCounter++;
	// 		} else {
	// 			LOG("TESTMODE: time elapsed\n");
	// 			if (was_global_read_ok > 0) {
	// 				LOG("WriteTendences\n");
	// 				WriteTendences = 1;
	// 			}
	// 			/* Reseta o contador */
	// 			testModeCounter = 0;
	// 		}
	// 	} else {
	// 		PreviousWrite = TendenceOpts.HasWrites;
	// 		CurrentTime = GetCurrentTime();
	// 		TendencePeriod = (TendenceOpts.HasWrites == 1 ?
	// 				TendenceOpts.PeriodConstant :
	// 				TendenceOpts.PeriodInitial);
	// 		char month0[80], month1[80];
	// 		GetTimeString(month0, 80, "%d/%m/%Y", CurrentTime);
	// 		GetTimeString(month1, 80, "%d/%m/%Y", TendenceOpts.LastWrite);
	// 		Months = GetDifferenceInMonths(CurrentTime, TendenceOpts.LastWrite);
	// 		LOG("Months [%s - %s] : %g => Used period: %d\n", month0, month1, Months, TendencePeriod);
	// 		/*
	// 		 * Only writes when the date period is reached AND we have a full read
	// 		 * otherwise we could get critical errors since if one battery fails
	// 		 * it will need to wait (several) months before the next read
	// 		 */
	// 		WriteTendences = (Months >= TendencePeriod && was_global_read_ok > 0 ? 1 : 0);
	// 	}
	// }

	for(int j = 0; j < str->string_size; j += 1){
		int global_id 		= str->string_id * str->string_size + j + 1;
		pt_vars 			= &(str->output_vars_read_last[j]);
		pt_state_current 	= &(str->batteries_states_curr[j]);
		pt_state_last 		= &(str->batteries_states_last[j]);
		pt_imp 				= &(str->output_vars_imp_last[j]);
		/* Alteração feita por Elielder para implementação de variável de corrente a fim de alimentar tabela DatalogRT no BD */
		int current_db      = current[str->string_id];
		int orientation_db	= orientation[str->string_id];

		evaluate_states(str, j, alarmconfig);
		
		int state = (str->batteries_read_states_curr[j] >= params.param3_messages_wait) ? 1 : 0;
		// LOG("Flavio Alves: cm_string_process_batteries: state[%d] = %d (%d / %d) %d\n",
		// 		j, state, str->batteries_read_states_curr[j], params.param3_messages_wait,
		// 		str->battery_mask.alarm_state[j]);
		if(state != 0){
			/* Flavio Alves: retirado com o intuito de garantir com que sempre
			 * seja enviada a informacao de tendencias, quando for sua hora */
			//str->string_ok = false;
			bits_set_bit(&(str->battery_mask), j, true);
			/* Avalia se e para enviar o alarme novamente.
			 * Muda o estado para envio de alarme caso se encontre no
			 * estado inicial  */
			if (str->battery_mask.alarm_state[j] != 1) {
				LOG("Flavio Alves: Setando flag de alarme\n");
				str->battery_mask.alarm_state[j] = 1;
				/* Flag que dispara efetivamente o envio de alarme */
				str->string_ok = false;
			} else {
				/* Caso a bateria esteja com problemas, a notificacao ja tiver
				 * sido enviada e for 8h00, o estado de envio do alarme e resetado
				 * para que uma nova mensagem possa ser enviada.
				 */
				time_t rawtime;   
				time ( &rawtime );
				struct tm *timeinfo = localtime (&rawtime);
				if ((timeinfo->tm_hour == 8) && (timeinfo->tm_min == 0)) {
					str->battery_mask.alarm_state[j] = 1;
					/* Flag que dispara efetivamente o envio de alarme */
					str->string_ok = false;
				}
			}
		} 
		// else {
		// 	/* Vai para o estado de idle */
		// 	str->battery_mask.alarm_state[j] = 0;
		// }

		err = db_add_response(pt_vars, pt_imp, pt_state_current,
		 				global_id, save_log_state, state, uaverage, current_db, orientation_db);

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

	// if(str->string_ok){
	// 	LOG("Flavio Alves: entrando nas tendencias %d %d\n",str->string_id, str->string_size);
	// 	for(int j = 0; j < str->string_size; j += 1){
	// 		//int global_id 		= str->string_id * str->string_size + j + 1;
	// 		pt_vars 			= &(str->output_vars_read_last[j]);
	// 		pt_state_current 	= &(str->batteries_states_curr[j]);
	// 		pt_state_last 		= &(str->batteries_states_last[j]);
	// 		pt_imp 				= &(str->output_vars_imp_last[j]);

	// 		if(WriteTendences == 1){
	// 			TendencesWrote &= cm_string_handle_tendence(pt_vars, pt_imp, pt_state_current,
	// 							 							CurrentTime, &TendenceOpts);
	// 		}
	// 	}
	// }
	
	db_update_average(uaverage, str->average_vars_curr.bus_sum, str->string_id);

	// if(WriteTendences == 1){
	// 	if (TendencesWrote){
	// 		TendenceOpts.HasWrites = 1;
	// 		TendenceOpts.LastWrite = GetCurrentTime();
	// 		TendenceOpts.LastIteration += 1;
	// 		db_update_tendence_configs(TendenceOpts);
	// 	}
	// 	else{
	// 		/*
	// 		 * TODO: Recover?
	// 		*/
	// 	}
	// }else{
	// 	LOG("Component::Not updating tendencies\n");
	// }

	return true;
}

/*
 * Process all batteries for battery-like alarms
*/
bool cm_string_process_eval_tendencies(int was_global_read_ok)
{
	// PTR_VALID(str);
	int err = 0;
	Protocol_States 					*pt_state_current;
	Protocol_States 					*pt_state_last;
	Protocol_ReadCmd_OutputVars			*pt_vars;
	Protocol_ImpedanceCmd_OutputVars 	*pt_imp;
	// unsigned int uaverage = _compressFloat(str->average_vars_curr.average);
	int3 dummy = {0};
	// str->string_ok = true;
	
	int WriteTendences = 0;
	int Months = 0;
	int PreviousWrite = 0;
	int TendencePeriod = 0;
	
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
			
			LOG("HasWrites:%d, PeriodConstant:%d, PeriodInitial:%d\n",
						TendenceOpts.HasWrites,TendenceOpts.PeriodConstant,
						TendenceOpts.PeriodInitial);
			TendencePeriod = (TendenceOpts.HasWrites == 1 ?
					TendenceOpts.PeriodConstant :
					TendenceOpts.PeriodInitial);
			char month0[80], month1[80];
			time_t CurrentTime = GetCurrentTime();
			GetTimeString(month0, 80, "%d/%m/%Y", CurrentTime);
			GetTimeString(month1, 80, "%d/%m/%Y", TendenceOpts.LastWrite);
			Months = GetDifferenceInMonths(month0, month1);
			LOG("Months [%s - %s] : %d => Used period: %d\n", month0, month1, Months, TendencePeriod);
			/*
			 * Only writes when the date period is reached AND we have a full read
			 * otherwise we could get critical errors since if one battery fails
			 * it will need to wait (several) months before the next read
			 */
			WriteTendences = 0;
			if (was_global_read_ok > 0) {
				if (Months < 0) {
					WriteTendences = 1;
				} else if (Months >= TendencePeriod) {
					WriteTendences = 1;
				}
			} 
			// 	WriteTendences = (Months >= TendencePeriod && was_global_read_ok > 0 ? 1 : 0);
			
		}
	}

	return (bool)WriteTendences;
}
	
bool cm_string_process_save_tendencies(cm_string_t *str, Database_Alarmconfig_t *alarmconfig,
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
	time_t CurrentTime;
	bool TendencesWrote = true;

	CurrentTime = GetCurrentTime();

	LOG("Flavio Alves: entrando nas tendencias %d %d\n",str->string_id, str->string_size);
	for(int j = 0; j < str->string_size; j += 1) {
		pt_vars 			= &(str->output_vars_read_last[j]);
		pt_state_current 	= &(str->batteries_states_curr[j]);
		pt_state_last 		= &(str->batteries_states_last[j]);
		pt_imp 				= &(str->output_vars_imp_last[j]);

		TendencesWrote &= cm_string_handle_tendence(pt_vars, pt_imp, pt_state_current, CurrentTime, &TendenceOpts);
	}

	return TendencesWrote;
}

bool cm_string_process_update_tendencies(void)
{
	TendenceOpts.HasWrites = 1;
	TendenceOpts.LastWrite = GetCurrentTime();
	TendenceOpts.LastIteration += 1;
	db_update_tendence_configs(TendenceOpts);

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