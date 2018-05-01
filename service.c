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

static int evaluate_states_results(
		Database_Alarmconfig_t *params,
		unsigned int target,
		unsigned int bus,
		unsigned int disk_capacity,
		Protocol_States *states) {

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
	// CCK_ZERO_DEBUG_V(dest);
}


int service_start(void) {
	int 				 i = 0, j = 0;
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
	StringAvg_t string_avg[MAX_STRING_LEN];
	StringAvg_t string_avg_last[MAX_STRING_LEN];
	int3 read_ok[MAX_STRING_LEN];

	bzero(&read_ok, sizeof(int3) * MAX_STRING_LEN);

	/*
	 * Atualiza o endereco MAC da placa
	 */
	if (CHECK(db_set_macaddress())) {
		return -1;
	}



	/*
	 * Inicia a execucao principal
	 */
	// average_last = params.average_last;
	/*
	 * Não consigo encontrar como a variavel average_last pode ser inicializada
	 * sem ser magicamente 0 pelo compilador, de modo que nao implementou-se 
	 * carregamento dos 'averages' por string
	*/

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

		LOG(SERVICE_LOG "Possui %d strings e %d bat/str\n", list.strings, list.batteries);

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
			LOG(SERVICE_LOG "total de sensores = %d\n",list.items);

			for(int ix = 0; ix < list.strings; ix += 1){
				int stringOK = 1;
				int lastStrState = read_ok[ix].i1;
				read_ok[ix].i1 = 0;
				read_ok[ix].i2 = 0;
				read_ok[ix].i3 = 0;
				string_avg[ix].average = 0.0f;
				string_avg[ix].bus_sum = 0.0f;
				
				for(int j = 0; j < list.batteries; j += 1){
					i = ix * list.batteries + j;
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
					input_vars.vref = string_avg_last[ix].average;
					/*
					 * Parametros seguintes se encontram na base
					 * de dados do sistema
					 */
					input_vars.duty_min = params.duty_min;
					/*
					 * Desligando equalização?
					*/
					input_vars.duty_max = params.duty_max;
					if(lastStrState != 0){
						input_vars.duty_max = 0;
					}
					
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
							if(stringOK != 0){
								stringOK = 0;
								read_ok[ix].i1 = err;
								read_ok[ix].i2 = ix;
								read_ok[ix].i3 = j;
								service_preserved_copy(&output_vars, &output_vars_last[i]);
							}
						}
						memcpy((unsigned char *)&output_vars_last[i], (unsigned char *)&output_vars, sizeof(Protocol_ReadCmd_OutputVars));

						// CCK_ZERO_DEBUG_V(&output_vars);
						// CCK_ZERO_DEBUG_F(&output_vars_last[i], i);
						err = prot_read_impedance(&input_impedance,&output_impedance, params.param3_messages_wait);
						if(err != 0){
							if(stringOK != 0){
								stringOK = 0;
								read_ok[ix].i1 = err;
								read_ok[ix].i2 = ix;
								read_ok[ix].i3 = j;
							}
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
						
						err = prot_read_vars(&input_vars,&output_vars, params.param3_messages_wait);
						if(err != 0){
							if(stringOK != 0){
								stringOK = 0;
								read_ok[ix].i1 = err;
								read_ok[ix].i2 = ix;
								read_ok[ix].i3 = j;
								service_preserved_copy(&output_vars, &output_vars_last[i]);
							}
						}

						/* Salva a leitura feita */
						memcpy((unsigned char *)&output_vars_last[i],(unsigned char *)&output_vars,sizeof(Protocol_ReadCmd_OutputVars));
						// CCK_ZERO_DEBUG_V(&output_vars);
						// CCK_ZERO_DEBUG_F(&output_vars_last[i], i);
						if (vars_read_counter >= params.num_cycles_var_read) {
							err = prot_read_impedance(&input_impedance,&output_impedance, params.param3_messages_wait);
							if(err != 0){
								if(stringOK != 0){
									stringOK = 0;
									read_ok[ix].i1 = err;
									read_ok[ix].i2 = ix;
									read_ok[ix].i3 = j;
								}
								memcpy((unsigned char *)&output_impedance, (unsigned char *)&output_impedance_last[i], sizeof(Protocol_ImpedanceCmd_OutputVars));
							}else{
								/* Salva a leitura feira */
								memcpy((unsigned char *)&output_impedance_last[i],(unsigned char *)&output_impedance,sizeof(Protocol_ImpedanceCmd_OutputVars));
							}
							// CCK_ZERO_DEBUG_E(&input_impedance);
						}
					}

					/*
					 * Calcula a media atualizada de vref
					 */
					// if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)) {
					float fvbat = (float)(output_vars.vbat);
					float fitems = (float)(list.batteries);
					string_avg[ix].average += fvbat / fitems;
					string_avg[ix].bus_sum += (unsigned int)output_vars.vbat;
					// }
				}
			}

			/*
			 * Salva as informacoes no banco de dados e calcula o estado das leituras
			 */
			for(int ix = 0; ix < list.strings; ix += 1){
				
				unsigned int uaverage = _compressFloat(string_avg[ix].average);

				for(int j = 0; j < list.batteries; j += 1){
					i = j + list.batteries * ix;
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
					err = db_add_response(pt_vars, pt_imp, pt_state_current, i+1, save_log_state,
										  read_ok[ix].i1, uaverage);
					if(err != 0){
						LOG("Erro na escrita do banco!\n");
					}
				}

				db_update_average(uaverage, string_avg[ix].bus_sum, ix);
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
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,TENSAO, read_ok[i]);
					}
					if (pt_state_current->temperatura != pt_state_last->temperatura) {
						/* Registra alarme de mudanca de temperatura */
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,TEMPERATURA, read_ok[i]);
					}
					if (pt_state_current->impedancia != pt_state_last->impedancia) {
						/* Registra alarme de mudanca de impedancia */
						db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig,IMPEDANCIA, read_ok[i]);
					}
				}
				/*
				 * Atualiza os valores antigos
				 */
				pt_state_last->tensao = pt_state_current->tensao;
				pt_state_last->temperatura = pt_state_current->temperatura;
				pt_state_last->impedancia = pt_state_current->impedancia;
			}

			if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)){
				/*
				 * Realiza a leitura da capacidade do disco, para
				 * enviar com as demais informacoes de tempo real
				 */
				capacity = disk_usedSpace("/");

				/*
				 * Persiste capacidade
				*/
				db_update_capacity(capacity);


				/*
				 * Verifica se alguma string gera alarme
				*/
				for(int ix = 0; ix < list.strings; ix += 1){
					unsigned int uaverage = _compressFloat(string_avg[ix].average);
					string_avg_last[ix].average = string_avg[ix].average;
					string_avg_last[ix].bus_sum = string_avg[ix].bus_sum;
					/*
					 * Processa os estados dos resultados calculados
					 */
					evaluate_states_results(&alarmconfig, uaverage, string_avg[ix].bus_sum, capacity,pt_state_current);

					if (!isFirstRead) {
						/*
						 * Podem ser gerados ate 3 alarmes por leitura, sendo um para cada leitura critica
						 */
						if(read_ok[ix].i1 != 0){
							/* Alarme relacionado a falha em alguma bateria na string */
							db_add_alarm(pt_vars,pt_imp,pt_state_current,&alarmconfig, STRING, read_ok[ix]);
						}
						if (pt_state_current->barramento != pt_state_last->barramento) {
							/* Registra alarme de mudanca de tensao de barramento */
							db_add_alarm_results(string_avg[ix].bus_sum,pt_state_current,&alarmconfig,BARRAMENTO);
						}
						if (pt_state_current->target != pt_state_last->target) {
							/* Registra alarme de mudanca de tensao target */
							db_add_alarm_results(uaverage,pt_state_current,&alarmconfig,TARGET);
						}
						if (pt_state_current->disk != pt_state_last->disk) {
							/* Registra alarme de capacidade de disco */
							db_add_alarm_results(capacity,pt_state_current,&alarmconfig,DISK);
						}
					}

					/*
					 * Atualiza os valores antigos
					 */
					pt_state_last->barramento = pt_state_current->barramento;
					pt_state_last->target = pt_state_current->target;
					pt_state_last->disk = pt_state_current->disk;
				}
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
