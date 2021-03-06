/*
 * main.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include <service.h>
#include <stdio.h>
#include <defs.h>

extern int DEBUG;

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
    LOG("Starting serial service\n");
	int err = 0;

	/*
	 * Inicializa o servico, a partir de seus parametros padrao de
	 * inicializacao. TODO: Recuperar a partir de argumentos de linha de
	 * comando
	 */
    
    if(argc != 3){
        LOG("Invalid arguments!\n");
    }
    
    char * device = argv[1];
    DEBUG = atoi(argv[2]);

    LOG("Looking for database...\n");
    waitWebInitialization();
    // LOG("Database found, initing service...\n");
	if(CHECK(service_init(device, NULL)) == 0){
        // LOG("Service ok, running...\n");
	    CHECK(service_start());
        service_finish();
    }else{
        LOG("Service unnable to init\n");
    }
	return err;
}
