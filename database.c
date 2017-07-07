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

#define DATABASE_LOG					"DATABASE:"
#define DATABASE_VARS_TABLE_NAME		"DataLog"
#define DATABASE_IMPEDANCE_TABLE_NAME	"impedance"
#define DATABASE_ADDRESSES_NAME			"Modulo"
#define DATABASE_PARAMETERS_TABLE_NAME	"Parameters"
#define DATABASE_NETWORK_TABLE_NAME		"RedeSeguranca"
#define DATABASE_PARAM_AVG_ADDR					1
#define DATABASE_PARAM_DUTYMIN_ADDR				2
#define DATABASE_PARAM_DUTYMAX_ADDR				3
#define DATABASE_PARAM_CTE_ADDR					4
#define DATABASE_PARAM_DELAY_ADDR				5
#define DATABASE_PARAM_NUM_CYCLES_VAR_READ		6
#define DATABASE_PARAM_BUS_SUM					7
#define DATABASE_PARAM_SAVE_LOG_TIME			8
#define DATABASE_PARAM_DISK_CAPACITY			9
#define DATABASE_PARAM_PARAM1_INTERBAT_DELAY	10
#define DATABASE_PARAM_PARAM2_SERIAL_READ_TO	11
#define DATABASE_PARAM_PARAM3_MESSAGES_WAIT		12
#define DATABASE_PARAM_NB_ITEMS					19+1
#define DATABASE_NETWORK_MAC_ADDR				1
#define DATABASE_NETWORK_NB_ITEMS				14+1

#define BATTERY_STRINGS_ADDR           4
#define BATTERY_COUNT_ADDR             5

static sqlite3 					*database;
static Database_Addresses_t		*addr_list;
static Database_Parameters_t	*param_list;
static sqlite3_stmt       		*baked_stmt;
static sqlite3_stmt				*baked_stmt_rt;
static char						mac_address[30];

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

	/*
	 * Sanity check
	 */
	if (argc < 6) {
		LOG(DATABASE_LOG "Problema na coleta da lista de baterias: %d\n",argc);
		addr_list->items = 0;
		return ret;
	}
	if(!param_list){
		LOG(DATABASE_LOG "read_callback: We have a nullptr\n");
		exit(0);
	}

	/*
	 * Tudo OK - realiza o procedimento
	 */
	if(addr_list->items == 0){ //this should run one time only
		int i = 0, j = 0;
		int bank_count = atoi(argv[BATTERY_STRINGS_ADDR]);
		int batt_count = atoi(argv[BATTERY_COUNT_ADDR]);
		int amount = bank_count * batt_count;

		/* Quantidade de strings se encontra na tabela de parametros,
		 * e deve ser atualizado a cada ciclo, de forma a saber quantos
		 * strings possui a configuracao e, dessa forma, calcular de
		 * forma correta a tensao de barramento */
		param_list->num_banks = bank_count;
		/* Preenche as estruturas para busca das informacoes dos sensores */
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
		LOG(DATABASE_LOG "Database unexpected result...\n");
	}

	return ret;
}

static int network_callback(void *data, int argc, char **argv, char **azColName){
	int ret = 0;

	memset(mac_address,0,sizeof(mac_address));

	/* Em caso do banco de dados vir com problema, de forma a nao chegar
	 * todos os parametros, eles serao carregados com valores padrao fixos */
	if (argc != DATABASE_NETWORK_NB_ITEMS) {
		LOG(DATABASE_LOG "Problemas na tabela - configura valor padrao 00:00:00:00:00:01\n");
		LOG(DATABASE_LOG "argc=%d / %d\n",argc,DATABASE_NETWORK_NB_ITEMS);
		strcpy(mac_address,"00:00:00:00:00:01");
		//system("ifconfig eth0 hw ether 00:00:00:00:00:01");
	} else {
		LOG(DATABASE_LOG "Atualizacao do MAC Address ... ");
		//sprintf(macaddr,"ifconfig eth0 hw ether %s\0",argv[DATABASE_NETWORK_MAC_ADDR]);
		//system(macaddr);
		memcpy(mac_address,argv[DATABASE_NETWORK_MAC_ADDR],17);
		LOG("OK\n");
	}

	return ret;
}

static int param_callback(void *data, int argc, char **argv, char **azColName){
	int ret = 0;
	char *garbage = 0;

	/*
	 * Sanity checks
	 */
	if(!param_list){
		LOG(DATABASE_LOG "param_callback:We have a nullptr\n");
		exit(0);
	}

	/* Em caso do banco de dados vir com problema, de forma a nao chegar
	 * todos os parametros, eles serao carregados com valores padrao fixos */
	if (argc != DATABASE_PARAM_NB_ITEMS) {
		LOG(DATABASE_LOG "Problemas na tabela - carregando valores padrao: %d\n",argc);
		param_list->average_last = 13500;
		param_list->duty_min = 0;
		param_list->duty_max = 45000;
		param_list->index = 0x2600;
		param_list->delay = 1;
		param_list->num_cycles_var_read = 500;
		param_list->disk_capacity = 0;
		param_list->param1_interbat_delay = 0;
		param_list->param2_serial_read_to = 0;
		param_list->param3_messages_wait = 3;
	} else {
		LOG(DATABASE_LOG "Leitura de valores da tabela de parametros\n");

		param_list->average_last = (unsigned short) strtol(argv[DATABASE_PARAM_AVG_ADDR], &garbage, 0);
		param_list->duty_min = (unsigned short) strtol(argv[DATABASE_PARAM_DUTYMIN_ADDR], &garbage, 0);
		param_list->duty_max = (unsigned short) strtol(argv[DATABASE_PARAM_DUTYMAX_ADDR], &garbage, 0);
		param_list->index = (unsigned short) strtol(argv[DATABASE_PARAM_CTE_ADDR], &garbage, 0);
		param_list->delay = (unsigned short) strtol(argv[DATABASE_PARAM_DELAY_ADDR], &garbage, 0);
		param_list->num_cycles_var_read = (unsigned short) strtol(argv[DATABASE_PARAM_NUM_CYCLES_VAR_READ], &garbage, 0);
		param_list->bus_sum = (unsigned int) strtol(argv[DATABASE_PARAM_BUS_SUM], &garbage, 0);
		param_list->save_log_time = (unsigned int) strtol(argv[DATABASE_PARAM_SAVE_LOG_TIME], &garbage, 0);
		param_list->disk_capacity = (unsigned int) strtol(argv[DATABASE_PARAM_DISK_CAPACITY], &garbage, 0);
		param_list->param1_interbat_delay = (unsigned int) strtol(argv[DATABASE_PARAM_PARAM1_INTERBAT_DELAY], &garbage, 0);
		param_list->param2_serial_read_to = (unsigned int) strtol(argv[DATABASE_PARAM_PARAM2_SERIAL_READ_TO], &garbage, 0);
		param_list->param3_messages_wait = (unsigned int) strtol(argv[DATABASE_PARAM_PARAM3_MESSAGES_WAIT], &garbage, 0);
	}

	LOG(DATABASE_LOG "Initing with:\n");
	LOG(DATABASE_LOG "AVG_LAST: %hu\n", param_list->average_last);
	LOG(DATABASE_LOG "DUTY_MIN: %hu\n", param_list->duty_min);
	LOG(DATABASE_LOG "DUTY_MAX: %hu\n", param_list->duty_max);
	LOG(DATABASE_LOG "INDEX: %hu\n", param_list->index);
	LOG(DATABASE_LOG "DELAY: %hu\n", param_list->delay);
	LOG(DATABASE_LOG "NUM_CYCLES_VAR_READ: %hu\n", param_list->num_cycles_var_read);
	LOG(DATABASE_LOG "BUS_SUM: %u\n",param_list->bus_sum);
	LOG(DATABASE_LOG "DISK_CAPACITY: %u\n",param_list->disk_capacity);
	LOG(DATABASE_LOG "PARAM1_INTERBAT_DELAY: %u\n",param_list->param1_interbat_delay);
	LOG(DATABASE_LOG "PARAM2_SERIAL_READ_TO: %u\n",param_list->param2_serial_read_to);
	LOG(DATABASE_LOG "PARAM3_MESSAGES_WAIT: %u\n",param_list->param3_messages_wait);

	return ret;
}

int initCallback(void *notUsed, int argc, char **argv, char **azColName){
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
        LOG(DATABASE_LOG "Unnable to open database, you might need to call support\n");
		return -1;
	}

    char *zErrMsg = 0;

	err = sqlite3_exec(database, DATABASE_BUSY_TIMEOUT, 0, 0, &zErrMsg);
	if(err != SQLITE_OK){
		LOG(DATABASE_LOG "Unable to set %s : %s\n",(char *)(DATABASE_BUSY_TIMEOUT), zErrMsg);
		sqlite3_close(database);
		return -1;
	}

	err = sqlite3_exec(database, DATABASE_JOURNAL_MODE, 0, 0, &zErrMsg);
	if(err != SQLITE_OK){
		LOG(DATABASE_LOG "Unable to set %s : %s\n", (char *)(DATABASE_JOURNAL_MODE), zErrMsg);
		sqlite3_close(database);
		return -1;
	}

	err = sqlite3_exec(database, "PRAGMA synchronous = OFF", 0, 0, &zErrMsg);
	if(err != SQLITE_OK){
		LOG(DATABASE_LOG "Unable to turn synchorinzation off : %s\n", zErrMsg);
		sqlite3_close(database);
		return -1;
	}

	sqlite3_prepare_v2(database, "INSERT INTO DataLog (dataHora, string, bateria, temperatura, impedancia, tensao, equalizacao) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1,
			&baked_stmt, NULL);
	sqlite3_prepare_v2(database, "UPDATE DataLogRT SET datahora = ?1, string = ?2, bateria = ?3, temperatura = ?4, impedancia = ?5, tensao = ?6, equalizacao = ?7 WHERE id = ?8;", -1, &baked_stmt_rt, NULL);

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
	database = 0;
	return 0;
}

int db_add_response(Protocol_ReadCmd_OutputVars *read_vars,
		Protocol_ImpedanceCmd_OutputVars *imp_vars, int id_db,
		int save_log)
{
	char timestamp[80];

	db_get_timestamp(timestamp);

	char etemp[15]; sprintf(etemp, "%hu", read_vars->etemp);
	char imped[15]; sprintf(imped, "%d", imp_vars->impedance);
	char vbat[15]; sprintf(vbat, "%hu", read_vars->vbat);
	char duty[15]; sprintf(duty, "%hu", read_vars->duty_cycle);
	char id[15]; sprintf(id,"%hu", id_db);

	//LOG("%d:%d:impedance = %d:%s\n",read_vars->addr_bank,read_vars->addr_batt,imp_vars->impedance,imped);

	/*
	 * Inclui campos na tabela de registros completa
	 */
	if (save_log) {
		LOG(DATABASE_LOG "Salvando DataLog ...");
		sqlite3_bind_text(baked_stmt, 1, timestamp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 2, int_to_addr(read_vars->addr_bank, 1), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 3, int_to_addr(read_vars->addr_batt, 0), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 4, etemp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 5, imped, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 6, vbat, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(baked_stmt, 7, duty, -1, SQLITE_TRANSIENT);
		sqlite3_step(baked_stmt);
		sqlite3_clear_bindings(baked_stmt);
		sqlite3_reset(baked_stmt);
		LOG("OK\n");
	}

	/*
	 * Inclui campos na tabela de registros de tempo real
	 */
	LOG(DATABASE_LOG "Salvando DataLogRT ...");
	if(read_vars->addr_bank == 0 || read_vars->addr_batt == 0){
		LOG("Unstable condition found!\n");
		EXT_PRINT("Error state:\n");
		prot_ext_print_info(read_vars);
		printf("Found an error!\n");
		exit(0);
	}
	sqlite3_bind_text(baked_stmt_rt, 1, timestamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 2, int_to_addr(read_vars->addr_bank, 1), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 3, int_to_addr(read_vars->addr_batt, 0), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 4, etemp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 5, imped, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 6, vbat, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 7, duty, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_stmt_rt, 8, id, -1, SQLITE_TRANSIENT);
	sqlite3_step(baked_stmt_rt);
	sqlite3_clear_bindings(baked_stmt_rt);
	sqlite3_reset(baked_stmt_rt);
	LOG("OK\n");

	return 0;
}


int db_add_vars(Protocol_ReadCmd_OutputVars *vars) {
	return 0;
}

int db_add_impedance(unsigned char addr_bank, unsigned char addr_batt,
		Protocol_ImpedanceCmd_OutputVars *vars) {
	return 0;
}

int db_get_addresses(Database_Addresses_t *list,Database_Parameters_t *p_list){
	if(database != 0){
		int err = 0;
		char sql_message[500];
		char *zErrMsg = 0;

		/*
		 * Inicializa ponteiro interno usado pela callbak e zera seu contador,
		 * pois será preenchido novamente
		 */
		addr_list = list;
		addr_list->items = 0;
		param_list = p_list;

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
			LOG(DATABASE_LOG "Error on select exec, msg: %s\n", zErrMsg);
			return -1;
		}

		return 0;
	}
	return -1;
}

int db_set_macaddress(void){
	if(database != 0){
		int err = 0;
		char sql_message[500];
		char system_cmd[100];
		char *zErrMsg = 0;

		sprintf(sql_message,
				"SELECT * FROM %s ",
				DATABASE_NETWORK_TABLE_NAME);
		err = sqlite3_exec(database,sql_message,network_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on select exec, msg: %s\n", zErrMsg);
			return -1;
		}

		LOG(DATABASE_LOG "Alterando MAC Address para %s ... ",mac_address);
		memset(system_cmd,0,sizeof(system_cmd));
		sprintf(system_cmd,"ifconfig eth0 hw ether %s",mac_address);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
		system(system_cmd);
#pragma GCC diagnostic pop
		LOG("OK\n");

		return 0;
	}
	return -1;
}

int db_get_parameters(Database_Parameters_t *list){
	if(database != 0){
		int err = 0;
		char sql_message[500];
		char *zErrMsg = 0;

		/*
		 * Inicializa ponteiro interno usado pela callbak e zera seu contador,
		 * pois será preenchido novamente
		 */
		param_list = list;

		/*
		 * Controi a mensagem SQL para o banco de dados
		 *
		 * TODO: Colocar a tabela e os parametros corretos
		 */

		sprintf(sql_message,
				"SELECT * FROM %s ",
				DATABASE_PARAMETERS_TABLE_NAME);
		err = sqlite3_exec(database,sql_message,param_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on select exec, msg: %s\n", zErrMsg);
			return -1;
		}

		return 0;
	}
	return -1;
}

int db_update_average(unsigned short new_avg, unsigned int new_sum, unsigned int capacity) {
	int err = 0;
	char sql[256];
	char *zErrMsg = 0;

	if (database != 0) {
		/*
		 * Atualizacao da informacao da tabela da tensao de target (media das tensoes)
		 */
		sprintf(sql,"UPDATE %s set avg_last = '%d', bus_voltage = %d, disk_capacity = %d;",DATABASE_PARAMETERS_TABLE_NAME, new_avg, new_sum, capacity);
		err = sqlite3_exec(database,sql,write_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on update exec, msg: %s\n",zErrMsg);
			return -1;
		}
		return 0;
	}
	return -1;
}
