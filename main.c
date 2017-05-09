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
    if (name == NULL) {
	return access(DEFAULT_DB_PATH, F_OK) != -1;
    } else {
        return access(name, F_OK) != -1;
    }
}

void waitWebInitialization(char *database){
    int exist = exist_file(database);
    int count = 0;
    while(!exist){
        LOG("Waiting for web to instanciate db [%d]\n", count++);
        sleep(1);
        exist = exist_file(database);
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
    
    if ((argc != 2) || (argc != 3)) {
        LOG("Invalid arguments!\n");
    }

    char * database = NULL;
    char * device = argv[1];
    LOG("Looking for database...\n");
    if (argc == 3) {
        database = argv[2];
    }
    waitWebInitialization(database);
    LOG("Database found, initing service...\n");
	if(CHECK(service_init(device, NULL)) == 0){
        LOG("Service ok, running...\n");
	    CHECK(service_start());
        service_finish();
    }else{
        LOG("Service unnable to init\n");
    }
	return err;
}
