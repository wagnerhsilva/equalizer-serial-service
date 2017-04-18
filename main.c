/*
 * main.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include <service.h>
#include <stdio.h>
#include <defs.h>


static int exist_file(const char *name){
    return access(name, F_OK) != -1;
}

void waitWebInitialization(void){
    int exist = exist_file(DEFAULT_DB_PATH);
    int count = 0;
    while(!exist){
        LOG("Waiting for web to instanciate db [%d]\n", count++);
        sleep(1);
        exist = exist_file(DEFAULT_DB_PATH);
    }
}

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
    
    waitWebInitialization();
    LOG("Database found, initing service...\n");
	if(CHECK(service_init(device, NULL)) == 0){
        LOG("Service ok, starting service...\n");
	    CHECK(service_start());
        service_finish();
    }else{
        LOG("Service unnable to init\n");
    }
	return err;
}
