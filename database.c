/*
 * database.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include <time.h>
#include <sqlite3.h>
#include "database.h"
#include "protocol.h"
#include <defs.h>
#include <string.h>
#define DATABASE_VARS_TABLE_NAME		"DataLog"
#define DATABASE_IMPEDANCE_TABLE_NAME	"impedance"
#define DATABASE_ADDRESSES_NAME			"Modulo"

#define BATTERY_STRINGS_ADDR              4
#define BATTERY_COUNT_ADDR                5

static sqlite3 				*database;
static Database_Addresses_t	*addr_list;

static int db_get_timestamp(char *timestamp){
	time_t rawtime;
	struct tm * timeinfo;

	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime (timestamp,80,"%G-%m-%d %H:%M:%S",timeinfo);

	return 0;
}

static int write_callback(void *data, int argc, char **argv, char **azColName){
   return 0;
}

static unsigned char addr_to_int(char *addr){
    char *end;
    unsigned char response;
    long i = 0;
    char *str = &addr[1];
    for(i = strtol(str, &end, 10); str != end; i = strtol(str, &end, 10)){ //grab last only
        str = end;
    }
    if(i < 256 && i >= 0) 
        response = (unsigned char) i;
    else
        response = 0;
    return response;
}

static char * int_to_addr(int val, int isBank){
    char * res = (char *)malloc(sizeof(char)*5); //at most 4 -> M255
    char *number = (char *)malloc(sizeof(char)*4);
    if(isBank){
        res[0] = 'S';
    }else{
        res[0] = 'M';
    }
    res[1] = '\0';
    
    snprintf(number, 4, "%d", val);
    strcat(res, number);
    free(number);
    return res;
}


static int read_callback(void *data, int argc, char **argv, char **azColName){
	/*
	 * We get the amount of strings in the database and the amount
	 * of batteries per string, since every string has the same amount
     * a simple loop should instanciate them all
	 */
    int ret = -1;
    if(addr_list->items == 0){ //this should run one time only
        int i = 0, j = 0;
        int bank_count = atoi(argv[BATTERY_STRINGS_ADDR]);
        int batt_count = atoi(argv[BATTERY_COUNT_ADDR]);
        
        int amount = bank_count * batt_count;
        if(amount < DATABASE_MAX_ADDRESSES_LEN){
            for(i = 0; i < bank_count; i++){
                for(j = 0; j < batt_count; j++){
                    addr_list->item[addr_list->items].addr_bank = (i+1);
                    addr_list->item[addr_list->items].addr_batt = (j+1);
                    
                    //TODO: Fill this with proper values, don't know where this comes from
                    addr_list->item[addr_list->items].vref = 0;
                    addr_list->item[addr_list->items].duty_min = 0;
                    addr_list->item[addr_list->items].duty_max = 0;
                    
                    addr_list->items++;
                }
            }
        ret = 0;
        }
   }else{
        LOG("Database unexpected result...\n");
    }
    
	return ret;
}

int db_init(char *path) {
	int err = 0;

	/*
	 * O banco de dados e inicializado, na forma de um arquivo. Caso o arquivo
	 * nao exista, ele e criado
	 */
	err = sqlite3_open(path,&database);
	if (err != 0) {
		return -1;
	}

	/*
	 * Assumindo que as tabelas ja existam e estejam prontas para serem usadas.
	 * Quem deve construir as tabelas e o sistema web. Na verdade, no
	 * processo de instalacao o arquivo com banco de dados ja deve vir
	 * previamente preparado.
	 */
	return 0;
}

int db_finish(void) {
	sqlite3_close(database);

	return 0;
}

int db_add_response(Protocol_ReadCmd_OutputVars *read_vars,
                    Protocol_ImpedanceCmd_OutputVars *imp_vars)
{
    int err = 0;
    char sql_message[500];
    char timestamp[80];
    char *zErrMsg = 0;
    
    err = db_get_timestamp(timestamp);
    sprintf(sql_message,
            "INSERT INTO %s (dataHora, string, bateria, temperatura, impedancia, tensao, equalizacao) VALUES ('%s', '%s', '%s', %hu, %d, %hu, %hu)", (char *)DATABASE_VARS_TABLE_NAME,
            timestamp, int_to_addr(read_vars->addr_bank, 1), int_to_addr(read_vars->addr_batt, 0), read_vars->etemp, 
            imp_vars->impedance, read_vars->vbat, read_vars->duty_cycle);
    err = sqlite3_exec(database,sql_message,write_callback,0,&zErrMsg);
	if (err != SQLITE_OK) {
        LOG("Error on insert into exec, msg: %s\n", zErrMsg);
		return -1;
	}
    
    return 0;
}


int db_add_vars(Protocol_ReadCmd_OutputVars *vars) {
//	int err = 0;
//	char sql_message[500];
//	char timestamp[16];
//	char *zErrMsg = 0;
//
//	/*
//	 * Captura a data e hora atual para registro
//	 */
//	err = db_get_timestamp(timestamp);
//
//	/*
//	 * Controi a mensagem SQL para o banco de dados
//	 *
//	 * TODO: Checar como foi implementada a politica de timestamp da tabela.
//	 * Caso nao tenha sido implementada, e preciso incluir na tabela.
//	 */
//	sprintf(sql_message,
//			"INSERT INTO %s (TIMESTAMP, BANK, BATTERY, VBAT, ITEMP, ETEMP, VSOURCE, OFF_VBAT, OFF_IBAT, VREF, DUTYCYCLE) "
//			"VALUES (%s, %u %u %u %u %u %u %u %u %u %u);",
//			DATABASE_VARS_TABLE_NAME,
//			timestamp,
//			vars->addr_bank,
//			vars->addr_batt,
//			vars->vbat,
//			vars->itemp,
//			vars->etemp,
//			vars->vsource,
//			vars->vbat_off,
//			vars->ibat_off,
//			vars->vref,
//			vars->duty_cycle);
//	err = sqlite3_exec(database,sql_message,write_callback,0,&zErrMsg);
//	if (err != SQLITE_OK) {
//		return -1;
//	}

	return 0;
}

int db_add_impedance(unsigned char addr_bank, unsigned char addr_batt,
		Protocol_ImpedanceCmd_OutputVars *vars) {
//	int err = 0;
//	char sql_message[500];
//	char timestamp[16];
//	char *zErrMsg = 0;
//
//	/*
//	 * Captura a data e hora atual para registro
//	 */
//	err = db_get_timestamp(timestamp);
//
//	/*
//	 * Controi a mensagem SQL para o banco de dados
//	 *
//	 * TODO: Checar como foi implementada a politica de timestamp da tabela.
//	 * Caso nao tenha sido implementada, e preciso incluir na tabela.
//	 */
//	sprintf(sql_message,
//			"INSERT INTO %s (TIMESTAMP, BANK, BATTERY, IMPEDANCE, CURRENT) "
//			"VALUES (%s, %u %u %u %u %u %u %u %u %u %d);",
//			DATABASE_IMPEDANCE_TABLE_NAME,
//			timestamp,
//			addr_bank,
//			addr_batt,
//			vars->impedance,
//			vars->current);
//	err = sqlite3_exec(database,sql_message,write_callback,0,&zErrMsg);
//	if (err != SQLITE_OK) {
//        LOG("Error on insert, msg: %s\n", zErrMsg);
//		return -1;
//	}

	return 0;
}

int db_get_addresses(Database_Addresses_t *list) {
	int err = 0;
	char sql_message[500];
	char *zErrMsg = 0;

	/*
	 * Inicializa ponteiro interno usado pela callbak e zera seu contador,
	 * pois será preenchido novamente
	 */
	addr_list = list;
	addr_list->items = 0;

	/*
	 * Controi a mensagem SQL para o banco de dados
	 *
	 * TODO: Checar como foi implementada a politica de timestamp da tabela.
	 * Caso nao tenha sido implementada, e preciso incluir na tabela.
	 */
 
	sprintf(sql_message,
			"SELECT * FROM %s ",
			DATABASE_ADDRESSES_NAME);
	err = sqlite3_exec(database,sql_message,read_callback,0,&zErrMsg);
	if (err != SQLITE_OK) {
        LOG("Error on select exec, msg: %s\n", zErrMsg);
		return -1;
	}

	return 0;
}
