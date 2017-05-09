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
	unsigned short			 average_last = 0;
	int 				 i = 0;
	int 				 err = 0;
	Database_Addresses_t		 list;
	Database_Parameters_t		 params;
	Protocol_ReadCmd_InputVars 	 input_vars;
	Protocol_ImpedanceCmd_InputVars	 input_impedance;
	Protocol_ReadCmd_OutputVars	 output_vars;
	Protocol_ImpedanceCmd_OutputVars output_impedance;

	/*
	 * Os parametros sao recuperados antes do inicio da execucao
	 * do loop principal
	 */
	if (CHECK(db_get_parameters(&params))) {
		return -1;
	}

	/*
	 * Inicia a execucao principal
	 */
	average_last = params.average_last;

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
		
		if (list.items > 0) {
			/*
			 * Busca informacao de variaveis e de impedancia para cada item
			 * presente na tabela
			 */
			
			f_average = 0;
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
				 * Dispara o processo de busca de informacoes no sensor
				 */
				err = prot_read_vars(&input_vars,&output_vars);
				if (err != 0) {
					break;
				}
				/*
				 * Armazena informacoes recebidas no banco de dados
				 */
				/*
				 * Preenche campos necessarios para solicitar leitura das
				 * informacoes de impedancia
				 */
				input_impedance.addr_bank = list.item[i].addr_bank;
				input_impedance.addr_batt = list.item[i].addr_batt;
				/*
				 * Dispara o processo de busca de informacoes no sensor
				 */
				err = prot_read_impedance(&input_impedance,&output_impedance);
				if (err != 0) {
					break;
				}
				/*
				 * Armazena as informacoes recebidas no banco de dados
				 */
				err = db_add_response(&output_vars, &output_impedance);
				if (err != 0) {
					break;
				}
				/*
				 * Calcula a media atualizada de vref
				 */
                float fvbat = (float)(output_vars.vbat);
                float fitems = (float)(list.items);
				f_average += fvbat / fitems; 
			}
			/*
			 * Atualiza o valor de vref medio usado como entrada
			 * na leitura dos sensores. Esta informacao deve ser
			 * armazenada em base de dados.
			 */
			average_last = _compressFloat(f_average);
			db_update_average(average_last);
	        //LOG("Storing average value : %g --> %u\n",f_average, average_last);		
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

