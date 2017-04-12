/*
 * main.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include <service.h>
#include <stdio.h>
#include <defs.h>

int main(int argc, char **argv) {

	int err = 0;

	/*
	 * Inicializa o servico, a partir de seus parametros padrao de
	 * inicializacao. TODO: Recuperar a partir de argumentos de linha de
	 * comando
	 */
    
    if(argc != 2){
        LOG("Invalid arguments!\n");
    }
    
    char * device = argv[1];

	CHECK(service_init(device, NULL));
	CHECK(service_start());
	service_finish();

	return err;
}
