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

#define DATABASE_VARS_TABLE_NAME		"readvars"
#define DATABASE_IMPEDANCE_TABLE_NAME	"impedance"
#define DATABASE_ADDRESSES_NAME			"addresses"

static sqlite3 				*database;
static Database_Addresses_t	*addr_list;

static int db_get_timestamp(char *timestamp){
	time_t rawtime;
	struct tm * timeinfo;

	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime (timestamp,16,"%G%m%d%H%M%S",timeinfo);

	return 0;
}

static int write_callback(void *data, int argc, char **argv, char **azColName){
   return 0;
}

static int read_callback(void *data, int argc, char **argv, char **azColName){
	/*
	 * Assumindo inicialmente que chegara somente o endereco do banco e da
	 * bateria
	 */
	if (addr_list->items < DATABASE_MAX_ADDRESSES_LEN) {
		addr_list->item[addr_list->items].addr_bank = atoi(argv[0]);
		addr_list->item[addr_list->items].addr_batt = atoi(argv[0]);
		addr_list->item[addr_list->items].vref = atoi(argv[0]);
		addr_list->item[addr_list->items].duty_min = atoi(argv[0]);
		addr_list->item[addr_list->items].duty_max = atoi(argv[0]);
		addr_list->items++;
	}

	return 0;
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

int db_add_vars(Protocol_ReadCmd_OutputVars *vars) {
	int err = 0;
	char sql_message[500];
	char timestamp[16];
	char *zErrMsg = 0;

	/*
	 * Captura a data e hora atual para registro
	 */
	err = db_get_timestamp(timestamp);

	/*
	 * Controi a mensagem SQL para o banco de dados
	 *
	 * TODO: Checar como foi implementada a politica de timestamp da tabela.
	 * Caso nao tenha sido implementada, e preciso incluir na tabela.
	 */
	sprintf(sql_message,
			"INSERT INTO %s (TIMESTAMP, BANK, BATTERY, VBAT, ITEMP, ETEMP, VSOURCE, OFF_VBAT, OFF_IBAT, VREF, DUTYCYCLE) "
			"VALUES (%s, %u %u %u %u %u %u %u %u %u %u);",
			DATABASE_VARS_TABLE_NAME,
			timestamp,
			vars->addr_bank,
			vars->addr_batt,
			vars->vbat,
			vars->itemp,
			vars->etemp,
			vars->vsource,
			vars->vbat_off,
			vars->ibat_off,
			vars->vref,
			vars->duty_cycle);
	err = sqlite3_exec(database,sql_message,write_callback,0,&zErrMsg);
	if (err != SQLITE_OK) {
		return -1;
	}

	return 0;
}

int db_add_impedance(unsigned char addr_bank, unsigned char addr_batt,
		Protocol_ImpedanceCmd_OutputVars *vars) {
	int err = 0;
	char sql_message[500];
	char timestamp[16];
	char *zErrMsg = 0;

	/*
	 * Captura a data e hora atual para registro
	 */
	err = db_get_timestamp(timestamp);

	/*
	 * Controi a mensagem SQL para o banco de dados
	 *
	 * TODO: Checar como foi implementada a politica de timestamp da tabela.
	 * Caso nao tenha sido implementada, e preciso incluir na tabela.
	 */
	sprintf(sql_message,
			"INSERT INTO %s (TIMESTAMP, BANK, BATTERY, IMPEDANCE, CURRENT) "
			"VALUES (%s, %u %u %u %u %u %u %u %u %u %u);",
			DATABASE_IMPEDANCE_TABLE_NAME,
			timestamp,
			addr_bank,
			addr_batt,
			vars->impedance,
			vars->current);
	err = sqlite3_exec(database,sql_message,write_callback,0,&zErrMsg);
	if (err != SQLITE_OK) {
		return -1;
	}

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
		return -1;
	}

	return 0;
}
