/*
 * main.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include "service.h"
#include <stdio.h>

int main(void) {

	int err = 0;

	/*
	 * Inicializa o servico, a partir de seus parametros padrao de
	 * inicializacao. TODO: Recuperar a partir de argumentos de linha de
	 * comando
	 */
	err = service_init(NULL,NULL);
	if (err != 0) {
		return 1;
	}

	err = service_start();
	if (err != 0) {
		err = 2;
	}

	service_finish();

	return err;
}
