/*
 * service.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include <service.h>
#include <signal.h>
#include <string.h>
#include <math.h>

#define SERVICE_UPDATE_PARAM_INTERVAL 	60
#define SERVICE_LOG						"SERVICE:"
Serial_t serial_comm;
static int stop = 1;

static void setStop(int value) {
	stop = value;
}

static int getStop(void) {
	return stop;
}

static int evaluate_states(
		Database_Alarmconfig_t *params,
		Protocol_ReadCmd_OutputVars *read_vars,
		Protocol_ImpedanceCmd_OutputVars *imp_vars,
		Protocol_States *states) {

	/*
	 * Tensao
	 */
	if (read_vars->vbat < params->tensao_min) {
		states->tensao = 1;
	} else if (read_vars->vbat > params->tensao_max) {
		states->tensao = 3;
	} else {
		states->tensao = 2;
	}

	/*
	 * Temperatura: o valor e com sinal (e possivel ter valores negativos)
	 * Com isso, o cast e necessario, uma vez que o dado recebido e tratado
	 * em numero bruto, sem considerar a sinalizacao (16 bits)
	 */
	if ((int)read_vars->etemp < params->temperatura_min) {
		states->temperatura = 1;
	} else if ((int)read_vars->etemp > params->temperatura_max) {
		states->temperatura = 3;
	} else {
		states->temperatura = 2;
	}

	/*
	 * Impedancia
	 */
	if (imp_vars->impedance < params->impedancia_min) {
		states->impedancia = 1;
	} else if (imp_vars->impedance > params->impedancia_max) {
		states->impedancia = 3;
	} else {
		states->impedancia = 2;
	}

	return 0;
}

int service_init(char *dev_path, char *db_path) {
	int err = 0;
	char *l_db = DEFAULT_DB_PATH;
	/*
	 * Checa o caminho para inicio da execucao
	 */
	if(dev_path == NULL){
		LOG("Invalid device path\n");
		return -1;
	}

	char *l_dev = dev_path;

	if (db_path != NULL) {
		l_db = db_path;
	}

	/*
	 * Inicializa a interface serial
	 */
	err = ser_init(&serial_comm,l_dev);
	if (err != 0) {
		return -1;
	}

	/*
	 * Inicializa o protocolo
	 */
	err = prot_init(&serial_comm);
	if (err != 0) {
		return -2;
	}

	/*
	 * Inicializa o banco de dados
	 */

	if(CHECK(db_init(l_db))){
		return -3;
	}
	/*
	 * Variavel que mantem o loop funcionando. A partir daqui o servico
	 * pode operar
	 */
	setStop(0);

	return 0;
}

//copy persisting address
void service_preserved_copy(Protocol_ReadCmd_OutputVars * dest, Protocol_ReadCmd_OutputVars * src){
	unsigned char bank = dest->addr_bank;
	unsigned char batt = dest->addr_batt;
	memcpy((unsigned char *)&dest[0], (unsigned char *)&src[0],sizeof(Protocol_ReadCmd_OutputVars));
	dest->addr_bank = bank;
	dest->addr_batt = batt;
	CCK_ZERO_DEBUG_V(dest);
}


int service_start(void) {
	float 				 f_average = 0.0f; //let's be on the safe side
	unsigned int			 f_bus_sum = 0;
	unsigned short			 average_last = 0;
	int 				 i = 0;
	int 				 err = 0;
	int				 vars_read_counter = 0;
	int				 update_param_counter = 0;
	int				 isFirstRead = 1;
	int				 save_log_counter = 0;
	int				 save_log_state = 1;
	int				 capacity = 0;
	Database_Addresses_t		 list;
	Database_Parameters_t		 params;
	Database_Alarmconfig_t		 alarmconfig;
	Protocol_ReadCmd_InputVars 	 input_vars;
	Protocol_ImpedanceCmd_InputVars	 input_impedance;
	Protocol_ReadCmd_OutputVars	 output_vars;
	Protocol_ReadCmd_OutputVars	 output_vars_last[MAX_STRING_LEN];
	Protocol_ReadCmd_OutputVars	 *pt_vars;
	Protocol_ImpedanceCmd_OutputVars output_impedance;
	Protocol_ImpedanceCmd_OutputVars output_impedance_last[MAX_STRING_LEN];
	Protocol_ImpedanceCmd_OutputVars *pt_imp;
	Protocol_States states[MAX_STRING_LEN];
	Protocol_States states_last[MAX_STRING_LEN];
	Protocol_States *pt_state_current;
	Protocol_States *pt_state_last;

	/*
	 * Atualiza o endereco MAC da placa
	 */
	if (CHECK(db_set_macaddress())) {
		return -1;
	}

	/*
	 * Inicia a execucao principal
	 */
	average_last = params.average_last;

	/*
	 * Atualiza a tabela de parametros, para o novo ciclo
	 * de execucao
	 */
	if (CHECK(db_get_parameters(&params,&alarmconfig))) {
		return -1;
	}
	/* Atualiza o timeout de leitura de serial */
	ser_setReadTimeout(&serial_comm,params.param2_serial_read_to);
	ser_setReadRetries(&serial_comm,params.param3_messages_wait);

	while(!getStop()) {
		/*
		 * Recupera a lista de elementos a serem recuperados
		 */

		if(CHECK(db_get_addresses(&list,&params))){
			break;
		}

		/*
		 * Avalia necessidade de atualizacao de informacoes de
		 * parametros.
		 */
		if (update_param_counter == SERVICE_UPDATE_PARAM_INTERVAL) {
			/* Atualiza parametros */
			if (CHECK(db_get_parameters(&params,&alarmconfig))) {
				break;
			}
			/* Atualiza o timeout de leitura de serial */
			ser_setReadTimeout(&serial_comm,params.param2_serial_read_to);
			ser_setReadRetries(&serial_comm,params.param3_messages_wait);
			/* reseta contador */
			update_param_counter = 0;
		} else {
			/* incrementa o contador */
			update_param_counter++;
		}

		/*
		 * Atualiza a ultima media calculada armazenada na base de
		 * dados
		 */
		if ((list.items > 0) && (list.items < MAX_STRING_LEN)) {
			/*
			 * Busca informacao de variaveis e de impedancia para cada item
			 * presente na tabela
			 */

			f_average = 0;
			f_bus_sum = 0;
			LOG(SERVICE_LOG "total de sensores = %d\n",list.items);
			for (i=0;i<list.items;i++){
				/*
				 * Inicializa as estrutura. As estruturas de saida nao sao
				 * zeradas, pois em caso de timeout o ultimo valor sera
				 * preservado.
				 */
				memset(&input_vars,0,sizeof(input_vars));
				memset(&input_impedance,0,sizeof(input_impedance));
				/*
				 * Preenche os campos necessarios para solicitar leitura das
				 * variaveis
				 */
				input_vars.addr_bank = list.item[i].addr_bank;
				input_vars.addr_batt = list.item[i].addr_batt;
				/*
				 * Vref e a media das leituras de tensao dos
				 * sensores, coletados a cada ciclo
				 */
				input_vars.vref = average_last;
				/*
				 * Parametros seguintes se encontram na base
				 * de dados do sistema
				 */
				input_vars.duty_min = params.duty_min;
				input_vars.duty_max = params.duty_max;
				input_vars.index = params.index;
				/*
				 * Preenche campos necessarios para solicitar leitura
				 * das informacoes de impedancia
				 */
				input_impedance.addr_bank = list.item[i].addr_bank;
				input_impedance.addr_batt = list.item[i].addr_batt;

				if (isFirstRead) {
					/*
					 * Se for a primeira leitura, realiza tanto a
					 * leitura das variaveis quanto a leitura da
					 * impedancia para toda a string
					 */
					err = prot_read_vars(&input_vars,&output_vars, params.param3_messages_wait);
					if(err != 0){
						service_preserved_copy(&output_vars, &output_vars_last[i]);
					}
					memcpy((unsigned char *)&output_vars_last[i], (unsigned char *)&output_vars, sizeof(Protocol_ReadCmd_OutputVars));

					// CCK_ZERO_DEBUG_V(&output_vars);
					// CCK_ZERO_DEBUG_F(&output_vars_last[i], i);
					err = prot_read_impedance(&input_impedance,&output_impedance, params.param3_messages_wait);
					if(err != 0){
						memcpy((unsigned char *)&output_impedance, (unsigned char *)&output_impedance_last[i], sizeof(Protocol_ImpedanceCmd_OutputVars));
					}else{
						/* Salva a leitura feira */
						memcpy((unsigned char *)&output_impedance_last[i],(unsigned char *)&output_impedance,sizeof(Protocol_ImpedanceCmd_OutputVars));
					}
					// CCK_ZERO_DEBUG_E(&input_impedance);
				} else {
					/* Seleciona a captura de informacoes, entre
					 * a busca por variaveis e a busca pela
					 * impedancia
					 */
					if (vars_read_counter < params.num_cycles_var_read) {
						err = prot_read_vars(&input_vars,&output_vars, params.param3_messages_wait);
						if(err != 0){
							service_preserved_copy(&output_vars, &output_vars_last[i]);
						}

						/* Salva a leitura feita */
						memcpy((unsigned char *)&output_vars_last[i],(unsigned char *)&output_vars,sizeof(Protocol_ReadCmd_OutputVars));
						// CCK_ZERO_DEBUG_V(&output_vars);
						// CCK_ZERO_DEBUG_F(&output_vars_last[i], i);
					} else {
						err = prot_read_impedance(&input_impedance,&output_impedance, params.param3_messages_wait);
						if(err != 0){
							memcpy((unsigned char *)&output_impedance, (unsigned char *)&output_impedance_last[i], sizeof(Protocol_ImpedanceCmd_OutputVars));
						}else{
							/* Salva a leitura feira */
							memcpy((unsigned char *)&output_impedance_last[i],(unsigned char *)&output_impedance,sizeof(Protocol_ImpedanceCmd_OutputVars));
						}
						// CCK_ZERO_DEBUG_E(&input_impedance);
					}
				}

				/*
				 * Armazena as informacoes recebidas no banco de dados
				 */

				/*
				 * Calcula a media atualizada de vref
				 */
				if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)) {
					float fvbat = (float)(output_vars.vbat);
					float fitems = (float)(list.items);
					f_average += fvbat / fitems;
					f_bus_sum += (unsigned int)output_vars.vbat;
				}

				/*
				 * Pausa entre as leituras das celulas
				 */
				sleep_ms(params.param1_interbat_delay);
			}

			/*
			 * Salva as informacoes no banco de dados e calcula o estado das leituras
			 */
			for (i=0;i<list.items;i++) {
				pt_vars = &output_vars_last[i];
				pt_imp = &output_impedance_last[i];
				pt_state_current = &states[i];
				/*
				 * Calcula o estado das leituras
				 */
				evaluate_states(&alarmconfig,pt_vars,pt_imp,pt_state_current);
				/*
				 * Armazena cada leitura no banco de dados
				 */
				// CCK_ZERO_DEBUG_V(pt_vars);
				err = db_add_response(pt_vars, pt_imp, pt_state_current, i+1, save_log_state);
				if(err != 0){
					LOG("Erro na escrita do banco!\n");
				}
			}

			/*
			 * Checa se novos alarmes devem ser gerados
			 */
			for (i=0;i<list.items;i++) {
				pt_vars = &output_vars_last[i];
				pt_imp = &output_impedance_last[i];
				pt_state_current = &states[i];
				pt_state_last = &states_last[i];

				if (!isFirstRead) {
					/*
					 * Podem ser gerados ate 3 alarmes por leitura, sendo um para cada leitura critica
					 */
					if (pt_state_current->tensao != pt_state_last->tensao) {
						/* Registra alarme de mudanca de tensao */
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,TENSAO);
					}
					if (pt_state_current->temperatura != pt_state_last->temperatura) {
						/* Registra alarme de mudanca de tensao */
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,TEMPERATURA);
					}
					if (pt_state_current->impedancia != pt_state_last->impedancia) {
						/* Registra alarme de mudanca de tensao */
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,IMPEDANCIA);
					}
				}
				/*
				 * Atualiza os valores antigos
				 */
				pt_state_last->tensao = pt_state_current->tensao;
				pt_state_last->temperatura = pt_state_current->temperatura;
				pt_state_last->impedancia = pt_state_current->impedancia;
			}

			/*
			 * Atualiza o valor de target e de tensao de barramento
			 * na base de dados, apos a captura de todas as leituras de
			 * sensores de uma string.
			 */
			if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)) {
				/* Realiza a checagem da quantidade de bancos, para realizar
				 * o calculo da tensão de barramento corretamente */
				if (params.num_banks > 1) {
					f_bus_sum = f_bus_sum / params.num_banks;
				}
				/*
				 * Realiza a leitura da capacidade do disco, para
				 * enviar com as demais informacoes de tempo real
				 */
				capacity = disk_usedSpace("/");
				/* Formata os dados e atualiza o banco de dados */
				average_last = _compressFloat(f_average);
				db_update_average(average_last, f_bus_sum, capacity);
				//LOG("Storing average value : %g --> %u\n",f_average, average_last);
			}
			/*
			 * Atualiza intervalo de registro na base de log
			 */
			if (save_log_state) {
				/* Checa se e para registrar sempre */
				if (params.save_log_time != 0) {
					save_log_state = 0;
					save_log_counter = 0;
				}
				/* Caso contrario, fica sempre na situacao de gravacao */
			} else {
				save_log_counter++;
				if (save_log_counter == params.save_log_time) {
					save_log_state = 1;
				}
			}
			/*
			 * Atualiza flag e contadores
			 */
			if (isFirstRead) {
				//LOG("Primeira Leitura realizada\n");
				isFirstRead = 0;
			}
			LOG(SERVICE_LOG "leitura %d realizada\n",vars_read_counter);
			if (vars_read_counter < params.num_cycles_var_read) {
				vars_read_counter++;
			} else {
				vars_read_counter = 0;
			}
			/*
			 * Realiza uma pausa entre as leituras, com valores
			 * lidos obtidos do banco de dados
			 */
			sleep_ms(params.delay);
			/*
			 * Reinicia loop de aquisicao de dados de sensor
			 */
		}
	}

	return err;
}

int service_stop(void) {
	/*
	 * Atualiza a variavel interna que mantem o funcionamento do loop
	 * principal. O objetivo aqui e permitir o final da execucao de forma
	 * natural, a partir de uma sinalizacao de encerramento do servico
	 */
	setStop(1);

	return 0;
}

int service_finish(void) {
	/*
	 * Finaliza os modulos serial e banco de dados. O modulo de protocolo
	 * nao e necessario.
	 */
	LOG("Closing database...");
	db_finish();
	LOG("closed.\n");
	LOG("Closing serial...");
	ser_finish(&serial_comm);
	LOG("closed.");
	return 0;
}
