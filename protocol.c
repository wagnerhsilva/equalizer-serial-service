/*
 * protocol.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include "protocol.h"
#include <string.h>
#include <stdint.h>

#define PROTOCOL_LOG				"PROTOCOL:"
#define PROTOCOL_START_OF_FRAME		0x2321
#define PROTOCOL_READ_VAR_COMMAND	0x10C0
#define PROTOCOL_IMPEDANCE_COMMAND	0x10B0

typedef struct {
	uint16_t start_frame;
	uint8_t  addr_bank;
	uint8_t  addr_batt;
	uint16_t command;
	uint16_t index;
	uint8_t  data[PROTOTOL_FRAME_REQUEST_DATA_LEN];
	uint16_t chksum;
} ProtocolRequest_t;

typedef struct {
	uint16_t command;
	uint16_t err_code;
	uint8_t data[PROTOCOL_FRAME_RESPONSE_DATA_LEN];
	uint16_t chksum;
} ProtocolResponse_t;

Serial_t *ser_instance;

static uint16_t prot_calc_checksum(uint8_t *data, int len) {
	int i = 0;
	uint16_t chksum = 0;


	/*
	 * Soma todos as words do buffer fornecido
	 */
	for (i=0;i<len;i += 2) {
        uint16_t value = bytes_to_u16(data[i+1], data[i]);
		chksum += value;
	}

    chksum = (~chksum) + 1;

	return chksum;
}

static uint8_t * prot_creat_readvar_request8(Protocol_ReadCmd_InputVars *in)
{
    /*
     * Implementacao alternativa ao malloc: nao foi encontrado o clean()
     * referente a esta chamada. Chamadas de malloc() sem clean() causam
     * problemas de memoria no longo prazo.
     *
     * Para se ter o mesmo efeito, mas de forma estatica, propoe-se o uso
     * de um buffer static interno.
     */

    //adicionado free ao final de prot_read_vars e prot_read_impedance
    //static arrays n�o fornecem a mesma versatilidade
    uint8_t *request8 = 0;
    request8 = (uint8_t*)malloc(sizeof(uint8_t)*PROTOCOL_FRAME_LEN);
    //static uint8_t request8[PROTOCOL_FRAME_LEN] = { 0 };
    int pointer = 0;

    request8[pointer++] = u16_MSB(PROTOCOL_START_OF_FRAME); //0x23
    request8[pointer++] = u16_LSB(PROTOCOL_START_OF_FRAME); //0x21
    request8[pointer++] = in->addr_bank; //bank
    request8[pointer++] = in->addr_batt; //batt

    request8[pointer++] = PROTOCOL_READ_VAR_COMMAND_0; //0x10
    request8[pointer++] = PROTOCOL_READ_VAR_COMMAND_1; //0xC0

    memset((void *)&(request8[pointer]), 0, PROTOTOL_FRAME_REQUEST_DATA_LEN); //clear data

    request8[pointer++] = u16_MSB(in->vref);
    request8[pointer++] = u16_LSB(in->vref);
    request8[pointer++] = u16_MSB(in->duty_max);
    request8[pointer++] = u16_LSB(in->duty_max);
    request8[pointer++] = u16_MSB(in->duty_min);
    request8[pointer++] = u16_LSB(in->duty_min);
    request8[pointer++] = u16_MSB(in->index);
    request8[pointer++] = u16_LSB(in->index);
    uint16_t chksum = prot_calc_checksum(&request8[2], 28);
    request8[30] = u16_MSB(chksum);
    request8[31] = u16_LSB(chksum);
    return request8;
}

void prot_ext_print_info(Protocol_ReadCmd_OutputVars *vars){
	EXT_PRINT("State: \n");
	EXT_PRINT("ERRCODE: %hu\n", vars->errcode);
	EXT_PRINT("VBAT: %hu\n", vars->vbat);
	EXT_PRINT("ETEMP: %hu\n", vars->etemp);
	EXT_PRINT("VSOURCE: %hu\n", vars->vsource);
	EXT_PRINT("ADDR_BANK: %d\n", vars->addr_bank);
	EXT_PRINT("ADDR_BATT: %d\n", vars->addr_batt);
}

static int prot_check_extract_readvar_response8(uint8_t * data,
		Protocol_ReadCmd_InputVars *in, Protocol_ReadCmd_OutputVars *out)
{
	uint8_t addr_bank = 0;
	uint8_t addr_batt = 0;
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 30);

    /* Em caso de timeout, o checksum calculado sera zero e o processo devera
       ter continuidade */
    if((markedChksum != chksum) && (markedChksum != 0)){
        // LOG(PROTOCOL_LOG "Invalid checksum, got: %04x expected: %04x\n",markedChksum, chksum);
        return -1;
    }

    /* Em caso de tratamento de mensagens de timeout, o comando recebido sera 0.
       Neste caso, o processamento deve seguir seu tratamento natural, que sera
       entregar valores zerados para a base de dados. */
    uint16_t cmd = bytes_to_u16(data[1], data[0]);
    if ((cmd != 0) && (cmd != PROTOCOL_READ_VAR_COMMAND)) {
        LOG(PROTOCOL_LOG "Incorrect command response, got: %04x expected: %04x\n", cmd, PROTOCOL_READ_VAR_COMMAND);
        return -2;
    }

    /* Confere se a mensagem de retorno corresponde a mensagem solicitada */
    addr_bank = data[22];
    addr_batt = data[23];
    if ((in->addr_bank != addr_bank) || (in->addr_batt != addr_batt)) {
    	LOG(PROTOCOL_LOG "Invalid message:request %02x%02x response %02x%02x\n",
    			in->addr_bank,in->addr_batt,addr_bank,addr_batt);
    	return -3;
    }

    out->errcode = bytes_to_u16(data[3], data[2]);
    out->vbat = bytes_to_u16(data[5], data[4]);
    out->itemp = bytes_to_u16(data[7], data[6]);
    out->etemp = bytes_to_u16(data[9], data[8]);
    out->vsource = bytes_to_u16(data[11], data[10]);
    out->hw_ver = data[12];
    out->fw_ver = data[13];
    out->vbat_off = bytes_to_u16(data[15], data[14]);
    out->ibat_off = bytes_to_u16(data[17], data[16]);
    out->vref = bytes_to_u16(data[19], data[18]);
    out->duty_cycle = bytes_to_u16(data[21], data[20]);
    out->addr_bank = data[22];
    out->addr_batt = data[23];

    //////////////////////////////////////////////////////
    //LOG("ERRCODE: %04x\n", out->errcode);
    //LOG("VBAT: %04x\n", out->vbat);
    //LOG("ITEMP: %04x\n", out->itemp);
    //LOG("ETEMP: %04x\n", out->etemp);
    //LOG("VSOURCE: %04x\n", out->vsource);
    //LOG("HWVER: %02x\n", out->hw_ver);
    //LOG("FWVER: %02x\n", out->fw_ver);
    //LOG("VBAT_OFF: %04x\n", out->vbat_off);
    //LOG("IBAT_OFF: %04x\n", out->ibat_off);
    //LOG("VREF: %04x\n", out->vref);
    //LOG("DUTY_CYCLE: %04x\n", out->duty_cycle);
    //LOG("ADDR_BANK: %02x\n", out->addr_bank);
    //LOG("ADDR_BATT: %02x\n", out->addr_batt);
    ///////////////////////////////////////////////////////

    return 0;
}

static uint8_t * prot_creat_impedance_request8(Protocol_ImpedanceCmd_InputVars *in)
{
    uint8_t *request8 = (uint8_t *)malloc(sizeof(uint8_t)*PROTOCOL_FRAME_LEN);
    int pointer = 0;

    request8[pointer++] = u16_MSB(PROTOCOL_START_OF_FRAME);
    request8[pointer++] = u16_LSB(PROTOCOL_START_OF_FRAME);
    request8[pointer++] = in->addr_bank;
    request8[pointer++] = in->addr_batt;
    request8[pointer++] = PROTOCOL_IMPEDANCE_COMMAND_0;
    request8[pointer++] = PROTOCOL_IMPEDANCE_COMMAND_1;

    memset((void *)&(request8[pointer]), 0, PROTOTOL_FRAME_REQUEST_DATA_LEN);
    uint16_t chksum = prot_calc_checksum(&request8[2], 28);
    request8[30] = u16_MSB(chksum);
    request8[31] = u16_LSB(chksum);
    return request8;
}

static int prot_check_extract_impedance_response8(uint8_t * data,
		Protocol_ImpedanceCmd_InputVars *in, Protocol_ImpedanceCmd_OutputVars *out)
{
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 30);

    if(markedChksum != chksum){
        // LOG("Invalid checksum, got: %d expected: %d\n", markedChksum, chksum);
        return -1;
    }

    /* Em caso de tratamento de mensagens de timeout, o comando recebido sera 0.
       Neste caso, o processamento deve seguir seu tratamento natural, que sera
       entregar valores zerados para a base de dados. */
    uint16_t cmd = bytes_to_u16(data[1], data[0]);
    if((cmd != 0) && (cmd != PROTOCOL_IMPEDANCE_COMMAND)){
        LOG("Incorrect command response, got: %d expected: %d\n", cmd, PROTOCOL_IMPEDANCE_COMMAND);
        return -2;
    }

    out->errcode = bytes_to_u16(data[3], data[2]);
    out->impedance = bytes_to_u32(data[4], data[5], data[6], data[7]);
    out->current = bytes_to_u32(data[8], data[9], data[10], data[11]);

    ////////////////////////////////////////////////////
    //LOG("Impedance message response:\n");
    //LOG("ERRCODE: %04x\n", out->errcode);
    //LOG("IMPEDANCE: %08x\n", out->impedance);
    //LOG("CURRENT: %08x\n", out->current);
    ////////////////////////////////////////////////////
    return 0;
}

static int prot_treat_impedance_timeout_response8(Protocol_ImpedanceCmd_InputVars *i,
                            Protocol_ImpedanceCmd_OutputVars *out)
{
    int err = 0;

    /* Anteriormente em caso de timeout era preciso retornar o valor
     * zero de leitura. Isso se mostrou inapropriado. Nesta atualizacao,
     * deve-se preservar o valor anterior. */

//    out->errcode = 0;
//    out->impedance = 0;
//    out->current = 0;

    ////////////////////////////////////////////////////
    //LOG("Impedance message response:\n");
    //LOG("ERRCODE: %04x\n", out->errcode);
    //LOG("IMPEDANCE: %08x\n", out->impedance);
    //LOG("CURRENT: %08x\n", out->current);
    ////////////////////////////////////////////////////
    return err;
}

//static int prot_communicate(uint8_t *msg8)
//{
//	int err = 0;
//
//	/*
//	 * Envia a mensagem pela serial. A serializacao do quadro sera feita
//	 * atraves de cast do tipo
//	 */
//	err = ser_write(ser_instance, msg8 ,PROTOCOL_FRAME_LEN);
//	if (err != 0) {
//		return -1;
//	}
//
//	/*
//	 * Aguarda a chegada da mensagem de resposta. A serializacao do quadro sera
//	 * feita atraves de cast do tipo
//	 */
//	/* Retirada a implementacao de timeout deste ponto. E implementada de forma
//	 * direta na estrutura de serial e pode ser parametrizada externamente pelo
//	 * usuario. TODO: Retirar
//	 */
//	uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t)*(PROTOCOL_FRAME_LEN+1));
//	err = ser_read(ser_instance, data, PROTOCOL_FRAME_LEN);
//	if (err == -3) {
//		/* Erro de timeout: retornar o codigo de erro especifico para
//		 * tratamento apropriado nas camadas superiores */
//		free(data);
//		return -3;
//	} else if (err == 0) {
//		/* Transfere a mensagem recebida para o buffer de retorno */
//		memcpy(msg8, data, sizeof(uint8_t)*(PROTOCOL_FRAME_LEN+1));
//	} else {
//		/* Erro */
//		free(data);
//		return -2;
//	}
//	free(data);
//	return 0;
//}

int prot_init(Serial_t *serial) {
	int err = 0;

	ser_instance = serial;

	/*
	 * Configura a interface serial, previamente inicializada
	 */
	err = ser_setup(ser_instance, 115200);
	if (err != 0) {
		return -2;
	}

	return 0;
}

void prot_gen_nill_out(Protocol_ReadCmd_InputVars *in,
		            Protocol_ReadCmd_OutputVars *out)
{
	out->addr_bank = in->addr_bank;
	out->addr_batt = in->addr_batt;
}

int prot_read_vars(Protocol_ReadCmd_InputVars *in,
		            Protocol_ReadCmd_OutputVars *out,
					int retries)
{
	int err = 0;
	int retry = retries;
	int stop = 0;
	uint8_t data[PROTOCOL_FRAME_LEN+1];

	/*
	 * Constroi a mensagem
	 */
	uint8_t *msg8 = prot_creat_readvar_request8(in);
	if (err != 0) {
		printf("prot_read_vars_RETURNING PREVIOUSLY!\n");
		return -1;
	}

	/*
	 * Envia a mensagem pela serial. A serializacao do quadro sera feita
	 * atraves de cast do tipo
	 */
	err = ser_write(ser_instance, msg8 ,PROTOCOL_FRAME_LEN);
	if (err != 0) {
		printf("prot_read_vars_RETURNING PREVIOUSLY!\n");
		return -1;
	}

	while ((retry > 0) && (stop == 0)) {
		err = ser_read(ser_instance, data, PROTOCOL_FRAME_LEN);
		if (err != 0){
			prot_gen_nill_out(in, out);
			break;
		}
		else {
			/*
			 * QUADRO RECEBIDO - Realiza a analise de integridade
			 */
			err = prot_check_extract_readvar_response8(data, in, out);
			if (err < 0) { // todos erros entram em retry
				/*
				 * QUADRO INVALIDO - aguarda pelo próximo quadro
				 */
				// LOG(PROTOCOL_LOG "Mensagem invalida, retries %d\n",retry);
				retry--;
			}else{
				break;
			}
		}
	}

	if (retry == 0) {
		err = -10;
	}

	free(msg8);
	// CCK_ZERO_DEBUG_V(out);
	return err;
}

int prot_read_impedance(Protocol_ImpedanceCmd_InputVars *in,
		                Protocol_ImpedanceCmd_OutputVars *out,
						int retries)
{
	int err = 0;
	int retry = retries;
	int stop = 0;
	uint8_t data[PROTOCOL_FRAME_LEN+1];

	/*
	 * Constroi a mensagem
	 */
	uint8_t *msg8 = prot_creat_impedance_request8(in);
	if (err != 0) {
		printf("prot_read_impedance_RETURNING PREVIOUSLY!\n");
		return -1;
	}

	/*
	 * Envia a mensagem pela serial. A serializacao do quadro sera feita
	 * atraves de cast do tipo
	 */
	err = ser_write(ser_instance, msg8 ,PROTOCOL_FRAME_LEN);
	if (err != 0) {
		printf("prot_read_impedance_RETURNING PREVIOUSLY!\n");
		return -1;
	}

	while ((retry > 0) && (stop == 0)) {
		/*
		 * Recebe o quadro pela serial
		 */
		err = ser_read(ser_instance, data, PROTOCOL_FRAME_LEN);
		if (err != 0) {
			/*
			 * TIMEOUT - mantem os ultimos dados lidos
			 */
			 err = prot_treat_impedance_timeout_response8(in,out);
			/* Sai do loop */
			break;
		} else {
			/*
			 * QUADRO RECEBIDO - Realiza a analise de integridade
			 */
			err = prot_check_extract_impedance_response8(data, in, out);
			if (err == -3) { //nunca executa
				/*
				 * QUADRO INVALIDO - aguarda pelo próximo quadro
				 * (Foi implementado igual a leitura de variaveis, mas esta
				 * situacao nunca sera obtida)
				 */
				// LOG(PROTOCOL_LOG "Mensagem invalida, retries %d\n",retry);
				retry--;
			} else {
				/*
				 * Qualquer outra situacao - sai do loop corretamente
				 * Em caso de erro de checksum ou de mensagem incorreta,
				 * ambas são descartadas no tratamento.
				 */
				break;
			}
		}
	}

	if (retry == 0) {
		err = -10;
	}

	/*
	 * Libera a mensagem criada para transmissao
	 */
	free(msg8);
	// CCK_ZERO_DEBUG_E(in);
	return err;
}

int _cck_zero_debug_vars_f(Protocol_ReadCmd_OutputVars *out, int index, int line, const char *file){
  if(out->addr_batt == 0 || out->addr_bank == 0){
    printf("Zero Value detected on line: %d of file: %s , index: %d\n",line, file, index);
    LOG("Zero Value detected on line: %d of file: %s , index: %d\n",line, file, index);
    EXT_PRINT("Zero Value detected on line: %d of file: %s , index: %d\n",line, file, index);
    exit(0);
  }
  return 0;
}

int _cck_zero_debug_vars(Protocol_ReadCmd_OutputVars *out, int line, const char *file){
  if(out->addr_batt == 0 || out->addr_bank == 0){
    printf("Zero Value detected on line: %d of file: %s\n",line, file);
    LOG("Zero Value detected on line: %d of file: %s\n", line, file);
    EXT_PRINT("Zero Value detected on line: %d of file: %s\n", line, file);
    exit(0);
  }
  return 0;
}

int _cck_zero_debug_impe(Protocol_ImpedanceCmd_InputVars *in, int line, const char *file){
  if(in->addr_batt == 0 || in->addr_bank == 0){
    printf("Zero Value detected on line: %d of file: %s\n",line, file);
    LOG("Zero Value detected on line: %d of file: %s\n", line, file);
    EXT_PRINT("Zero Value detected on line: %d of file: %s\n", line, file);
    exit(0);
  }
  return 0;
}
