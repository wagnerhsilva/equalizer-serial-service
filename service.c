/*
d * service.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include "database.h"
#include "protocol.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>

#define DEFAULT_SERIAL_PATH "/dev/ttyUSB0"
#define DEFAULT_DB_PATH "database.db"

static Serial_t serial_comm;
static int stop = 1;

static void setStop(int value) {
	stop = value;
}

static int getStop(void) {
	return stop;
}

int service_init(const char *dev_path, const char *db_path) {
	int err = 0;
	char *l_dev = DEFAULT_SERIAL_PATH;
	char *l_db = DEFAULT_DB_PATH;

	/*
	 * Checa o caminho para inicio da execucao
	 */
	if (dev_path != NULL) {
		l_dev = dev_path;
	}
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
	err = db_init(l_db);
	if (err != 0) {
		return -3;
	}

	/*
	 * Variavel que mantem o loop funcionando. A partir daqui o servico
	 * pode operar
	 */
	setStop(1);

	return 0;
}

int service_start(void) {
	int 								i = 0;
	int 								err = 0;
	Database_Addresses_t				list;
	Protocol_ReadCmd_InputVars 			input_vars;
	Protocol_ImpedanceCmd_InputVars		input_impedance;
	Protocol_ReadCmd_OutputVars			output_vars;
	Protocol_ImpedanceCmd_OutputVars	output_impedance;

	while(!getStop()) {
		/*
		 * Recupera a lista de elementos a serem recuperados
		 */
		err = db_get_addresses(&list);
		if (err != 0) {
			break;
		}

		if (list.items > 0) {
			/*
			 * Busca informacao de variaveis e de impedancia para cada item
			 * presente na tabela
			 */
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
				input_vars.vref = list.item[i].vref;
				input_vars.duty_min = list.item[i].duty_min;
				input_vars.duty_max = list.item[i].duty_max;
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
				err = db_add_vars(&output_vars);
				if (err != 0) {
					break;
				}
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
				err = db_add_impedance(input_impedance.addr_bank,
						input_impedance.addr_batt,&output_impedance);
				if (err != 0) {
					break;
				}
			}
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
	db_finish();
	ser_finish(&serial_comm);

	return 0;
}

