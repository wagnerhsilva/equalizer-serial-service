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
#include <manager.h>

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

int service_start(void) {
	int 				 		err = 0;
	int				 			vars_read_counter = 0;
	int				 			update_param_counter = 0;
	int				 			isFirstRead = 1;
	int				 			save_log_counter = 0;
	int				 			save_log_state = 1;
	int				 			capacity = 0;
	int 						global_read_ok = 0;
	bool						discharge_mode = false;
	Database_Addresses_t		list;
	Database_Parameters_t		params;
	Database_Alarmconfig_t		alarmconfig;
	extern Idioma_t				idioma;

	/*
	 * Atualiza o endereco MAC da placa
	 */
	if (CHECK(db_set_macaddress())) {
		return -1;
	}

	/*
	 * Atualiza o idioma padrao
	 */
	if (CHECK(db_get_language(&idioma))) {
		return -1;
	}
	/*
	 * Atualiza a tabela de parametros, para o novo ciclo
	 * de execucao
	 */
	if (CHECK(db_get_parameters(&params,&alarmconfig))) {
		return -1;
	}
	/*
	 * Atualiza o fuso horario
	 */
	if (CHECK(db_update_timezone())) {
		return -1;
	}
	/* Atualiza o timeout de leitura de serial */
	ser_setReadTimeout(&serial_comm,params.param2_serial_read_to);
	ser_setReadRetries(&serial_comm,params.param3_messages_wait);

	/*
	 * Cria a estrutura controladora de leituras
	*/
	cm_manager_t *manager = 0;
	if(!cm_manager_new(&manager)){
		LOG("Manager::Falha na inicialização!\n");
		exit(1);
	}

	while(!getStop()) {
		/*
		 * Recupera a lista de elementos a serem recuperados
		 */
		if(CHECK(db_get_addresses(&list,&params))){
			break;
		}

		(void) db_get_tendence_configs(&TendenceOpts);

		/*
		 * Caso ocorra troca da estrutura de leitura devido a atualizações
		 * vamos recomeçar como se fosse a primeira leitura, isso evita
		 * bias na geração de alarmes
		*/
		if(cm_manager_setup(&manager, list.strings, list.batteries)){
			isFirstRead = 1;
		}

		LOG(SERVICE_LOG "Possui %d strings e %d bat/str\n",
		 cm_manager_string_count(manager),
		 cm_manager_batteries_per_string_count(manager));

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
			/* Atualiza o idioma */
			if (CHECK(db_get_language(&idioma))) {
				break;
			}
			/* Atualiza o timezone */
			if (CHECK(db_update_timezone())) {
				break;
			}
		} else {
			/* incrementa o contador */
			update_param_counter++;
		}

		/*
		 * Atualiza a ultima media calculada armazenada na base de
		 * dados
		 */
		if (cm_manager_batteries_count(manager) > 0) {
			/*
			 * Busca informacao de variaveis e de impedancia para cada item
			 * presente na tabela
			 */
			LOG(SERVICE_LOG "total de sensores = %d\n", cm_manager_batteries_count(manager));

			/*
			 * Quando isFirstRead = true, essa chamada faz leitura de
			 * ambas variáveis VARS e IMPE. Quando é false precisa-se
			 * testar pelo ciclo de leitura
			*/
			bool readSuccess = true;
			bool readStatus = cm_manager_read_strings(manager, 
													 isFirstRead,
													 params, 
													 VARS, &readSuccess);
			/*
			 * Testa se precisa fazer invocação manual do processo de leitura
			 * de impedancia
			*/
			bool manual = (vars_read_counter == params.num_cycles_var_read);
			if(!isFirstRead && manual){
				readStatus |= cm_manager_read_strings(manager, 
													  false,
													  params,
													  IMPE, &readSuccess);
			}

			/*
			 * Apenas se leu alguma coisa processa salvamento e disparo
			 * de alarmes
			*/
			if(readStatus){
				/*
				 * Dispara escrita de respostas de baterias e alarmes
				 * de baterias
				*/
				cm_manager_process_batteries(manager, &alarmconfig, params,
											 save_log_state, isFirstRead,
											 readSuccess ? 1 : 0);

				/*
				 * Dispara escrita de respostas de strings e alarmes 
				 * de strings
				*/

				capacity = disk_usedSpace("/");

				/*
				 * Persiste capacidade
				*/
				db_update_capacity(capacity);

				cm_manager_process_strings(manager, &alarmconfig, params,
											capacity, isFirstRead);
				
				/*
				 * Verifica o modo de descarga
				 */
				discharge_mode = cm_manager_evaluate_discharge_mode(manager,params);
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
				LOG("Evaluating save log: current=%d, discharge_mode=%d, max=%d|%d\n",
						save_log_counter, discharge_mode, params.save_log_time, params.param9_discharge_mode_rate);
				if (discharge_mode) {
					if (save_log_counter >= params.param9_discharge_mode_rate) {
						save_log_state = 1;
					}
				} else {
					if (save_log_counter >= params.save_log_time) {
						save_log_state = 1;
					}
				}
				LOG("save_log_state = %d\n");
			}
			/*
			 * Atualiza flag e contadores
			 */
			if (isFirstRead) {
				//LOG("Primeira Leitura realizada\n");
				isFirstRead = 0;
				/* Informa a web que atualização pendente foi resolvida */
				if(is_file(UPDATED_FILE)){
					remove(UPDATED_FILE);
				}
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
	// LOG("closed.\n");
	// LOG("Closing serial...");
	ser_finish(&serial_comm);
	LOG("closed.");
	return 0;
}
