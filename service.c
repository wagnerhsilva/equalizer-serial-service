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

Serial_t serial_comm;
static int stop = 1;

static void setStop(int value) {
	stop = value;
}

static int getStop(void) {
	return stop;
}

void signal_handler(int signo){
	const char *signame = strsignal(signo);
	LOG("Received signal: %s\n", signame);
	LOG("Ending service...\n");
	service_finish();
}

static void init_signal_handler(void){
	struct sigaction new_action, old_action;
	new_action.sa_handler = signal_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	sigaction(SIGINT, &new_action, NULL);
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
	float 				 f_average = 0.0f; //let's be on the safe side
	unsigned int			 f_bus_sum = 0;
	unsigned short			 average_last = 0;
	unsigned int			 bus_sum_last = 0;
	int 				 i = 0;
	int 				 err = 0;
	int				 vars_read_counter = 0;
	int				 isFirstRead = 1;
	int				 save_log_counter = 0;
	int				 save_log_state = 1;
	Database_Addresses_t		 list;
	Database_Parameters_t		 params;
	Protocol_ReadCmd_InputVars 	 input_vars;
	Protocol_ImpedanceCmd_InputVars	 input_impedance;
	Protocol_ReadCmd_OutputVars	 output_vars;
	Protocol_ReadCmd_OutputVars	 output_vars_last[MAX_STRING_LEN];
	Protocol_ReadCmd_OutputVars	 *pt_vars;
	Protocol_ImpedanceCmd_OutputVars output_impedance;
	Protocol_ImpedanceCmd_OutputVars output_impedance_last[MAX_STRING_LEN];
	Protocol_ImpedanceCmd_OutputVars *pt_imp;

	/*
	 * Os parametros sao recuperados antes do inicio da execucao
	 * do loop principal
	 */
	if (CHECK(db_get_parameters(&params))) {
		return -1;
	}

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
	bus_sum_last = params.bus_sum;

	LOG("INICIAL: save_log_state = %d\n",save_log_state);
	LOG("INICIAL: save_log_counter = %d\n",save_log_counter);
	while(!getStop()) {
		/*
		 * Recupera a lista de elementos a serem recuperados
		 */

		if(CHECK(db_get_addresses(&list))){
			break;
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
			LOG("total de sensores = %d\n",list.items);
			for (i=0;i<list.items;i++) {
				/*
				 * Inicializa as estrutura
				 */
				memset(&input_vars,0,sizeof(input_vars));
				memset(&output_vars,0,sizeof(output_vars));
				memset(&input_impedance,0,sizeof(input_impedance));
				memset(&output_impedance,0,sizeof(output_impedance));
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
					err = prot_read_vars(&input_vars,&output_vars);
					if (err != 0) {
						break;
					}
					/* Salva a leitura feita */
					memcpy((unsigned char *)&output_vars_last[i],(unsigned char *)&output_vars,sizeof(Protocol_ReadCmd_OutputVars));
					err = prot_read_impedance(&input_impedance,&output_impedance);
					if (err != 0) {
						break;
					}
					/* Salva a leitura feira */
					memcpy((unsigned char *)&output_impedance_last[i],(unsigned char *)&output_impedance,sizeof(Protocol_ImpedanceCmd_OutputVars));
				} else {
					/* Seleciona a captura de informacoes, entre
					 * a busca por variaveis e a busca pela
					 * impedancia
					 */
					if (vars_read_counter < params.num_cycles_var_read) {
						err = prot_read_vars(&input_vars,&output_vars);
						if (err != 0) {
							break;
						}
						/* Salva a leitura feita */
						memcpy((unsigned char *)&output_vars_last[i],(unsigned char *)&output_vars,sizeof(Protocol_ReadCmd_OutputVars));
					} else {
						err = prot_read_impedance(&input_impedance,&output_impedance);
						if (err != 0) {
							break;
						}
						/* Salva a leitura feira */
						memcpy((unsigned char *)&output_impedance_last[i],(unsigned char *)&output_impedance,sizeof(Protocol_ImpedanceCmd_OutputVars));

					}
				}

				/*
				 * Armazena as informacoes recebidas no banco de dados
				 */
				if (isFirstRead) {
					//LOG("isFirstRead:output_vars:output_impedance\n");
					/* Na primeira leitura, as duas leituras sao realizadas e armazenadas */
					pt_vars = &output_vars;
					pt_imp = &output_impedance;
				} else {
					if (vars_read_counter < params.num_cycles_var_read) {
						//LOG("notFirst:output_vars:output_impedance_last\n");
						/* Registra ultima leitura de impedancia */
						pt_vars = &output_vars;
						pt_imp = &output_impedance_last[i];
					} else {
						//LOG("notFirst:output_vars_last:output_impedance\n");
						/* Registra ultima leitura de variavel */
						pt_vars = &output_vars_last[i];
						pt_imp = &output_impedance;
					}
				}
				err = db_add_response(pt_vars, pt_imp, i+1, save_log_state);
				if (err != 0) {
					break;
				}
				/*
				 * Calcula a media atualizada de vref
				 */
				if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)) {
					float fvbat = (float)(output_vars.vbat);
					float fitems = (float)(list.items);
					f_average += fvbat / fitems; 
					f_bus_sum += (unsigned int)output_vars.vbat;
				}
			}
			/*
			 * Atualiza o valor de target e de tensao de barramento
			 * na base de dados, apos a captura de todas as leituras de
			 * sensores de uma string
			 */
			if ((isFirstRead) || (vars_read_counter < params.num_cycles_var_read)) {
				average_last = _compressFloat(f_average);
				bus_sum_last = f_bus_sum;
				db_update_average(average_last, f_bus_sum);
				//LOG("Storing average value : %g --> %u\n",f_average, average_last);
			}
			/*
			 * Atualiza intervalo de registro na base de log
			 */
			LOG("save_log_state = %d\n",save_log_state);
			LOG("save_log_counter = %d\n",save_log_counter);
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
			LOG("vars_read_counter = %d\n",vars_read_counter);
			if (vars_read_counter < params.num_cycles_var_read) {
				vars_read_counter++;
			} else {
				LOG("%d - zerando contador\n",vars_read_counter);
				vars_read_counter = 0;
			}
			/*
			 * Realiza uma pausa entre as leituras, com valores
			 * lidos obtidos do banco de dados
			 */
			sleep(params.delay);
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

