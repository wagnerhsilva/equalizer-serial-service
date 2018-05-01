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
static Database_Alarmconfig_t	*alarmconfig_list;
static sqlite3_stmt       		*baked_stmt;
static sqlite3_stmt				*baked_stmt_rt;
static sqlite3_stmt				*baked_alarmlog;
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

		/*
		 * Persiste a quantidade de strings e baterias/string
		*/
		addr_list->strings = bank_count;
		addr_list->batteries = batt_count;

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
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		param_list->average_last = (unsigned short) strtol(argv[DATABASE_PARAM_AVG_ADDR], &garbage, 0);
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		param_list->duty_min = (unsigned short) strtol(argv[DATABASE_PARAM_DUTYMIN_ADDR], &garbage, 0);
		param_list->duty_max = (unsigned short) strtol(argv[DATABASE_PARAM_DUTYMAX_ADDR], &garbage, 0);
		param_list->index = (unsigned short) strtol(argv[DATABASE_PARAM_CTE_ADDR], &garbage, 0);
		param_list->delay = (unsigned short) strtol(argv[DATABASE_PARAM_DELAY_ADDR], &garbage, 0);
		param_list->num_cycles_var_read = (unsigned short) strtol(argv[DATABASE_PARAM_NUM_CYCLES_VAR_READ], &garbage, 0);
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		param_list->bus_sum = (unsigned int) strtol(argv[DATABASE_PARAM_BUS_SUM], &garbage, 0);
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

static int alarmconfig_callback(void *data, int argc, char **argv, char **azColName){
	int ret = 0;
	char *garbage = 0;

	/*
	 * Sanity checks
	 */
	if(!alarmconfig_list){
		LOG(DATABASE_LOG "alarmconfig_callback:We have a nullptr\n");
		exit(0);
	}

	/* Em caso do banco de dados vir com problema, de forma a nao chegar
	 * todos os parametros, eles serao carregados com valores padrao fixos */
	if (argc != 10) {
		LOG(DATABASE_LOG "Problemas na tabela Alarmconfig - carregando valores padrao: %d\n",argc);
		alarmconfig_list->impedancia_max = 0;
		alarmconfig_list->impedancia_min = 0;
		alarmconfig_list->tensao_max = 0;
		alarmconfig_list->tensao_min = 0;
		alarmconfig_list->temperatura_max = 0;
		alarmconfig_list->temperatura_min = 0;
		alarmconfig_list->barramento_max = 0;
		alarmconfig_list->barramento_min = 0;
		alarmconfig_list->target_max = 0;
		alarmconfig_list->target_min = 0;
	} else {
		LOG(DATABASE_LOG "Leitura de valores da tabela de parametros\n");
		alarmconfig_list->tensao_max = (unsigned int)(strtof(argv[0],&garbage) * 1000);
		alarmconfig_list->tensao_min = (unsigned int)(strtof(argv[1],&garbage) * 1000);
		alarmconfig_list->temperatura_max = (int)(strtof(argv[2],&garbage) * 10);
		alarmconfig_list->temperatura_min = (int)(strtof(argv[3],&garbage) * 10);
		alarmconfig_list->impedancia_max = (unsigned int)(strtof(argv[4],&garbage) * 100);
		alarmconfig_list->impedancia_min = (unsigned int)(strtof(argv[5],&garbage) * 100);
		alarmconfig_list->barramento_max = (unsigned int)(strtof(argv[6],&garbage) * 1000);
		alarmconfig_list->barramento_min = (unsigned int)(strtof(argv[7],&garbage) * 1000);
		alarmconfig_list->target_max = (unsigned int)(strtof(argv[8],&garbage) * 1000);
		alarmconfig_list->target_min = (unsigned int)(strtof(argv[9],&garbage) * 1000);
	}

	LOG(DATABASE_LOG "Initing with:\n");
	LOG(DATABASE_LOG "TENSAO_MAX: %d\n", alarmconfig_list->tensao_max);
	LOG(DATABASE_LOG "TENSAO_MIN: %d\n", alarmconfig_list->tensao_min);
	LOG(DATABASE_LOG "TEMPERATURA_MAX: %d\n", alarmconfig_list->temperatura_max);
	LOG(DATABASE_LOG "TEMPERATURA_MIN: %d\n", alarmconfig_list->temperatura_min);
	LOG(DATABASE_LOG "IMPEDANCIA_MAX: %d\n", alarmconfig_list->impedancia_max);
	LOG(DATABASE_LOG "IMPEDANCIA_MIN: %d\n", alarmconfig_list->impedancia_min);
	LOG(DATABASE_LOG "BARRAMENTO_MAX: %d\n", alarmconfig_list->barramento_max);
	LOG(DATABASE_LOG "BARRAMENTO_MIN: %d\n", alarmconfig_list->barramento_min);
	LOG(DATABASE_LOG "TARGET_MAX: %d\n", alarmconfig_list->target_max);
	LOG(DATABASE_LOG "TARGET_MIN: %d\n", alarmconfig_list->target_min);

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

	sqlite3_prepare_v2(database, "INSERT INTO DataLog (dataHora, string, bateria, temperatura, impedancia, tensao, equalizacao, target) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);", -1, &baked_stmt, NULL);
	sqlite3_prepare_v2(database, "INSERT OR IGNORE INTO DataLogRT (id, datahora, string, bateria, temperatura, impedancia, tensao, equalizacao, batstatus) VALUES (?8, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?9); UPDATE DataLogRT SET datahora = ?1, string = ?2, bateria = ?3, temperatura = ?4, impedancia = ?5, tensao = ?6, equalizacao = ?7, batstatus = ?9 WHERE id = ?8;", -1, &baked_stmt_rt, NULL);
	sqlite3_prepare_v2(database, "INSERT INTO AlarmLog (dataHora, descricao, emailEnviado, n_ocorrencias) VALUES (?1, ?2, ?3, ?4);", -1, &baked_alarmlog, NULL);

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
		Protocol_ImpedanceCmd_OutputVars *imp_vars,
		Protocol_States *states,
		int id_db,
		int save_log,
		int ok,
		unsigned int str_target)
{
	// CCK_ZERO_DEBUG_V(read_vars);
	char *zErrMsg = 0;
	char timestamp[80];

	db_get_timestamp(timestamp);

	char etemp[15]; sprintf(etemp, "%hu", read_vars->etemp);
	char imped[15]; sprintf(imped, "%d", imp_vars->impedance);
	char vbat[15]; sprintf(vbat, "%hu", read_vars->vbat);
	char duty[15]; sprintf(duty, "%hu", read_vars->duty_cycle);
	char id[15]; sprintf(id,"%hu", id_db);
	char s_tensao[15]; sprintf(s_tensao, "%d", states->tensao);
	char s_temperatura[15]; sprintf(s_temperatura, "%d", states->temperatura);
	char s_impedancia[15]; sprintf(s_impedancia, "%d", states->impedancia);
	char chr_str_targ[15]; sprintf(chr_str_targ, "%hu", str_target);

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
		sqlite3_bind_text(baked_stmt, 8, chr_str_targ, -1, SQLITE_TRANSIENT);
		sqlite3_step(baked_stmt);
		sqlite3_clear_bindings(baked_stmt);
		sqlite3_reset(baked_stmt);
		LOG("OK\n");
	}

	/*
	 * TODO: Corrigir implementação
	 * Inclui campos na tabela de registros de tempo real
	 */
	LOG(DATABASE_LOG "Salvando DataLogRT ...");
	char sql[2048];
	snprintf(sql, 2048, "INSERT OR IGNORE INTO DataLogRT "
		"(id, datahora, string, bateria, temperatura, impedancia, tensao, equalizacao, batstatus) "
		"VALUES (%s, '%s', '%s', '%s', %s, %s, %s, %s, %d); UPDATE DataLogRT SET datahora = '%s', string = '%s', "
		"bateria = '%s', temperatura = %s, impedancia = %s, tensao = %s, equalizacao = %s, batstatus = %d WHERE id = %s;", id,
		timestamp, int_to_addr(read_vars->addr_bank, 1), int_to_addr(read_vars->addr_batt, 0), 
		etemp, imped, vbat, duty, ok, timestamp, int_to_addr(read_vars->addr_bank, 1), int_to_addr(read_vars->addr_batt, 0), 
		etemp, imped, vbat, duty, ok, id);

	int err = sqlite3_exec(database,sql,write_callback,0,&zErrMsg);
	if(err != SQLITE_OK){
		LOG("Erro %s\nSQL: %s\n", zErrMsg, sql);
		exit(1);
	}
	LOG("OK\n");

	return 0;
}

int db_add_alarm_results(unsigned int value,
		Protocol_States *states,
		Database_Alarmconfig_t *alarmconfig,
		Protocol_States_e tipo)
{
	// CCK_ZERO_DEBUG_V(read_vars);
	char timestamp[80];
	char message[256];
	char l_min[15];
	char l_max[15];
	char l_medida[15];

	db_get_timestamp(timestamp);

	/*
	 * Construcao da mensagem de alarme
	 */
	switch(tipo) {
	case BARRAMENTO:
		if (states->barramento == 1) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(l_min,"%3d.%3d",(alarmconfig->barramento_min/1000),(alarmconfig->barramento_min%1000));
			sprintf(message,"Alerta de tensao no barramento, Minima %s de %s",
					l_medida,l_min);
		} else if (states->barramento == 2) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(message,"Alerta de tensao no barramento, dentro da faixa %s",
					l_medida);
		} else if (states->barramento == 3) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(l_max,"%3d.%3d",(alarmconfig->barramento_max/1000),(alarmconfig->barramento_max%1000));
			sprintf(message,"Alerta de tensao no barramento, Maxima %s de %s",
					l_medida,l_max);
		}
		break;
	case TARGET:
		if (states->target == 1) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(l_min,"%3d.%3d",(alarmconfig->target_min/1000),(alarmconfig->target_min%1000));
			sprintf(message,"Alerta de target, Minima %s de %s",
					l_medida,l_min);
		} else if (states->target == 2) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(message,"Alerta de target, dentro da faixa %s",
					l_medida);
		} else if (states->target == 3) {
			sprintf(l_medida,"%3d.%3d",(value/1000),(value%1000));
			sprintf(l_max,"%3d.%3d",(alarmconfig->target_max/1000),(alarmconfig->target_max%1000));
			sprintf(message,"Alerta de target, Maxima %s de %s",
					l_medida,l_max);
		}
		break;
	case DISK:
		if (states->disk == 2) {
			sprintf(message,"Alerta de capacidade de disco, abaixo de 95%%");
		} else if (states->disk == 3) {
			sprintf(message,"Alerta de capacidade de disco, acima de 95%%");
		}
		break;
	}

	/*
	 * Inclui campos na tabela de registros de tempo real
	 */
	LOG(DATABASE_LOG "Registrando alarme ...\n");
	LOG(DATABASE_LOG "Mensagem: %s\n", message);
	sqlite3_bind_text(baked_alarmlog, 1, timestamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 2, message, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 3, "0", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 4, "1", -1, SQLITE_TRANSIENT);
	sqlite3_step(baked_alarmlog);
	sqlite3_clear_bindings(baked_alarmlog);
	sqlite3_reset(baked_alarmlog);
	LOG("Alarme registrado\n");

	return 0;
}



int db_add_alarm(Protocol_ReadCmd_OutputVars *read_vars,
		Protocol_ImpedanceCmd_OutputVars *imp_vars,
		Protocol_States *states,
		Database_Alarmconfig_t *alarmconfig,
		Protocol_States_e tipo,
		int3 read_st)
{
	// CCK_ZERO_DEBUG_V(read_vars);
	char timestamp[80];
	char message[256];
	char l_min[15];
	char l_max[15];
	char l_medida[15];

	db_get_timestamp(timestamp);

	/*
	 * Construcao da mensagem de alarme
	 */
	switch(tipo) {
	case TENSAO:
		if (states->tensao == 1) {
			sprintf(l_medida,"%3d.%3d",(read_vars->vbat/1000),(read_vars->vbat%1000));
			sprintf(l_min,"%3d.%3d",(alarmconfig->tensao_min/1000),(alarmconfig->tensao_min%1000));
			sprintf(message,"Alerta de tensao em %s-%s, Minima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_min);
		} else if (states->tensao == 2) {
			sprintf(l_medida,"%3d.%3d",(read_vars->vbat/1000),(read_vars->vbat%1000));
			sprintf(message,"Alerta de tensao em %s-%s, dentro da faixa %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida);
		} else if (states->tensao == 3) {
			sprintf(l_medida,"%3d.%3d",(read_vars->vbat/1000),(read_vars->vbat%1000));
			sprintf(l_max,"%3d.%3d",(alarmconfig->tensao_max/1000),(alarmconfig->tensao_max%1000));
			sprintf(message,"Alerta de tensao em %s-%s, Maxima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_max);
		}
		break;
	case TEMPERATURA:
		if (states->temperatura == 1) {
			sprintf(l_medida,"%3d.%1d",(read_vars->etemp/10),(read_vars->etemp%10));
			sprintf(l_min,"%3d.%1d",(alarmconfig->temperatura_min/10),(alarmconfig->temperatura_min%10));
			sprintf(message,"Alerta de temperatura em %s-%s, Minima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_min);
		} else if (states->temperatura == 2) {
			sprintf(l_medida,"%3d.%1d",(read_vars->etemp/10),(read_vars->etemp%10));
			sprintf(message,"Alerta de temperatura em %s-%s, dentro da faixa %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida);
		} else if (states->temperatura == 3) {
			sprintf(l_medida,"%3d.%1d",(read_vars->etemp/10),(read_vars->etemp%10));
			sprintf(l_max,"%3d.%1d",(alarmconfig->temperatura_max/10),(alarmconfig->temperatura_max%10));
			sprintf(message,"Alerta de temperatura em %s-%s, Maxima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_max);
		}
		break;
	case IMPEDANCIA:
		if (states->impedancia == 1) {
			sprintf(l_medida,"%3d.%2d",(imp_vars->impedance/100),(imp_vars->impedance%100));
			sprintf(l_min,"%3d.%2d",(alarmconfig->impedancia_min/100),(alarmconfig->impedancia_min%100));
			sprintf(message,"Alerta de impedancia em %s-%s, Minima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_min);
		} else if (states->impedancia == 2) {
			sprintf(l_medida,"%3d.%2d",(imp_vars->impedance/100),(imp_vars->impedance%100));
			sprintf(message,"Alerta de impedancia em %s-%s, dentro da faixa %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida);
		} else if (states->impedancia == 3) {
			sprintf(l_medida,"%3d.%2d",(imp_vars->impedance/100),(imp_vars->impedance%100));
			sprintf(l_max,"%3d.%2d",(alarmconfig->impedancia_max/100),(alarmconfig->impedancia_max%100));
			sprintf(message,"Alerta de impedancia em %s-%s, Maxima %s de %s",
					int_to_addr(read_vars->addr_bank, 1),
					int_to_addr(read_vars->addr_batt, 0),
					l_medida,l_max);
		}
		break;
	case STRING:{
			char *p0 = int_to_addr(read_st.i2+1, 1);
			char *p1 = int_to_addr(read_st.i3+1, 0);
			snprintf(message, 256, "Alerta de erro de leitura em %s-%s com erro: %d",
						p0, p1, read_st.i1);
			free(p0);
			free(p1);
		}
		break;
	}

	/*
	 * Inclui campos na tabela de registros de tempo real
	 */
	LOG(DATABASE_LOG "Registrando alarme ...\n");
	LOG(DATABASE_LOG "Mensagem: %s\n", message);
	sqlite3_bind_text(baked_alarmlog, 1, timestamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 2, message, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 3, "0", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(baked_alarmlog, 4, "1", -1, SQLITE_TRANSIENT);
	sqlite3_step(baked_alarmlog);
	sqlite3_clear_bindings(baked_alarmlog);
	sqlite3_reset(baked_alarmlog);
	LOG("Alarme registrado\n");

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

int db_get_parameters(Database_Parameters_t *list, Database_Alarmconfig_t *alarmconfig){
	if(database != 0){
		int err = 0;
		char sql_message[500];
		char *zErrMsg = 0;

		/*
		 * Inicializa ponteiro interno usado pela callbak e zera seu contador,
		 * pois será preenchido novamente
		 */
		param_list = list;
		alarmconfig_list = alarmconfig;

		/*
		 * Busca informacoes da tabela de parametros
		 */
		sprintf(sql_message,
				"SELECT * FROM %s ",
				DATABASE_PARAMETERS_TABLE_NAME);
		err = sqlite3_exec(database,sql_message,param_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on select exec, msg: %s\n", zErrMsg);
			return -1;
		}

		/*
		 * Busca informacoes da tabela alarmconfig
		 */
		sprintf(sql_message,
				"SELECT "
				"alarme_nivel_tensao_max, alarme_nivel_tensao_min,"
				"alarme_nivel_temp_max, alarme_nivel_temp_min,"
				"alarme_nivel_imped_max,alarme_nivel_imped_min,"
				"alarme_nivel_tensaoBarr_max, alarme_nivel_tensaoBarr_min,"
				"alarme_nivel_target_max, alarme_nivel_target_min "
				" FROM AlarmeConfig");
		err = sqlite3_exec(database,sql_message,alarmconfig_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on select exec, msg: %s\n", zErrMsg);
			return -2;
		}

		return 0;
	}
	return -1;
}

int db_update_average(unsigned short new_avg, unsigned int new_sum, int id) {
	int err = 0;
	char sql[256];
	char *zErrMsg = 0;

	if (database != 0) {
		/*
		 * Atualizacao da informacao da tabela da tensao de target (media das tensoes)
		 */
		sprintf(sql,"INSERT OR IGNORE INTO Medias (id, tensao, target) VALUES (%u, %u, %d);"
					" UPDATE Medias SET tensao=%u, target=%u WHERE id=%d;",
		 	   id, new_sum, new_avg, new_sum, new_avg, id);

		err = sqlite3_exec(database,sql,write_callback,0,&zErrMsg);
		if (err != SQLITE_OK) {
			LOG(DATABASE_LOG "Error on average update, msg: %s\n",zErrMsg);
			return -1;
		}
		return 0;
	}
	return -1;
}

int db_update_capacity(unsigned int capacity){
	int err  = 0;
	char sql[256];
	char *zErrMsg = 0;
	if(database != 0){
		sprintf(sql, "UPDATE %s set disk_capacity = %d;", DATABASE_PARAMETERS_TABLE_NAME, capacity);
		err = sqlite3_exec(database, sql, write_callback, 0, &zErrMsg);
		if(err != SQLITE_OK){
			LOG(DATABASE_LOG "Error on disk capacity update, msg: %s\n", zErrMsg);
			return -1;
		}
		return 0;
	}
	return -1;
}