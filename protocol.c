/*
 * protocol.c
 *
 *  Created on: Apr 9, 2017
 *      Author: flavio
 */

#include "protocol.h"
#include <string.h>
#include <stdint.h>

#define PROTOCOL_START_OF_FRAME		0x2320
#define PROTOCOL_READ_VAR_COMMAND	0x10C0
#define PROTOCOL_IMPEDANCE_COMMAND	0x10B0

typedef struct {
	uint16_t start_frame;
	uint8_t  addr_bank;
	uint8_t  addr_batt;
	uint16_t command;
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

//static int prot_verify_checksum(ProtocolResponse_t *msg) {
//	union serialization_short {
//		ProtocolResponse_t msg;
//		uint16_t buf[PROTOCOL_FRAME_LEN_SHORT];
//	};
//
//	union serialization_short msg_r;
//	int i = 0;
//	unsigned short checksum = 0;
//
//	/*
//	 * Transferindo o conteudo externo para a variavel interna
//	 */
//	memcpy(&msg_r.msg,msg,sizeof(ProtocolResponse_t));
//
//	for(i=1;i<PROTOCOL_FRAME_LEN_SHORT;i++) {
//		checksum += msg_r.buf[i];
//	}
//
//	/*
//	 * A soma de todos os elementos do quadro, excluindo o Start of Frame e
//	 * incluindo o Checksum, deve ser zero
//	 */
//	if (checksum == 0) {
//		/* Retorna quadro valido */
//		return 1;
//	} else {
//		/* Quadro invalido */
//		return 0;
//	}
//}

static uint8_t * prot_creat_readvar_request8(Protocol_ReadCmd_InputVars *in)
{
    uint8_t *request8 = 0;
    request8 = (uint8_t*)malloc(sizeof(uint8_t)*PROTOCOL_FRAME_LEN);
    int pointer = 0;
    
    request8[pointer++] = u16_MSB(PROTOCOL_START_OF_FRAME); //0x23
    request8[pointer++] = u16_LSB(PROTOCOL_START_OF_FRAME); //0x20
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
    uint16_t chksum = prot_calc_checksum(&request8[2], 28);
    request8[30] = u16_MSB(chksum);
    request8[31] = u16_LSB(chksum);
    return request8;
}

//static int prot_creat_readvar_request(ProtocolRequest_t *request,
//		                            Protocol_ReadCmd_InputVars *in) 
//{
//
//	union conv_short {
//		uint16_t val;
//		uint8_t buf[2];
//	} serialization;
//
//	/*
//	 * Preenchendo informacoes dos campos iniciais do quadro
//	 */
//	request->start_frame = PROTOCOL_START_OF_FRAME;
//	request->addr_bank = in->addr_bank;
//	request->addr_batt = in->addr_batt;
//	request->command = PROTOCOL_READ_VAR_COMMAND;
//
//	/*
//	 * Preenchendo informacoes do campo de dados
//	 */
//	memset(request->data,0,PROTOTOL_FRAME_REQUEST_DATA_LEN);
//	serialization.val = in->vref;
//	memcpy(&request->data[0],serialization.buf,2);
//	serialization.val = in->duty_max;
//	memcpy(&request->data[2],serialization.buf,2);
//	serialization.val = in->duty_min;
//	memcpy(&request->data[4],serialization.buf,2);
//
//	/*
//	 * Calculando checksum. Sera passado o endereco inicial do quadro a ser
//	 * enviado deslocado dos dois primeiros bytes, indicadores do inicio de
//	 * quadro (Start Of Frame). O calculo sera feito em words (2 bytes),
//	 * portanto a quantidade de dados deve ser a metade da quantidade de bytes
//	 * efetivos
//	 */
//	//request->chksum = prot_calc_checksum((uint16_t *)
//	//		(((uint8_t *)request)+2), 14);
//
//	return 0;
//}


static int prot_check_extract_readvar_response8(uint8_t * data,Protocol_ReadCmd_OutputVars *out)
{
    int err = 0;
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 28);
    
    if(markedChksum != chksum){
        LOG("Invalid checksum, got: %d expected: %d\n",markedChksum, chksum);
        return -1;
    }
    
    uint16_t cmd = bytes_to_u16(data[1], data[0]);
    if(cmd != PROTOCOL_READ_VAR_COMMAND){
        LOG("Incorrect command response, got: %d expected: %d\n", cmd, PROTOCOL_READ_VAR_COMMAND);
        return -2;
    }

    int pointer = 2;
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

//static int prot_check_extract_readvar_response(ProtocolResponse_t *msg,
//		Protocol_ReadCmd_OutputVars *out) 
//{
//	union conv_short {
//		uint16_t val;
//		uint8_t buf[2];
//	};
//	int err = 0;
//	union conv_short deserial;
//
//	/*
//	 * Primeiro passo e checar se o quadro recebido esta correto.
//	 */
//	err = prot_verify_checksum(msg);
//	if (err == 0) {
//		return -1;
//	}
//
//	/*
//	 * Segundo passo e checar se a resposta corresponde ao comando enviado
//	 */
//	if (msg->command != PROTOCOL_READ_VAR_COMMAND) {
//		return -2;
//	}
//
//	/*
//	 * A partir de um quadro valido, extrair os parametros de resposta. De
//	 * acordo com a documentacao, o campo data esta organizado em words,
//	 * contendo as seguintes informacoes:
//	 * - BYTES V e VI – Tensão de Bateria: Valor a ser mostrado no campo VBAT
//	 * na WEB
//	 * - BYTES VII e VIII – Temperatura Interna: Não usado interface WEB
//	 * - BYTES IX e X – Temperatura Externa: Valor a ser mostrado campo TEMP WEB
//	 * - BYTES XI e XII – Tensão Fonte: Não usado interface WEB
//	 * - BYTES XIII e XIV – Versão de HARDWARE e FIRMWARE: Versão hardware 1,
//	 * versão software 3(0x03). Valor a ser mostrado no campo de versão de
//	 * software e hardware da placa na WEB.
//	 *  - BYTES XV e XVI – Valor do OFFSET de tensão de bateria: Não usado na
//	 *  interface WEB
//	 *  - BYTES XVII e XVIII – Valor do OFFSET de corrente de bateria: Não usado
//	 *  na interface WEB
//	 *  - BYTES XIX e XX ‐ Tensão de Referência usada
//	 *  - BYTES XXI e XXII ‐ DUTY CYCLE ATUAL: Valor a ser mostrado no campo %
//	 *  de equalização da WEB. Este valor deve ser divido por 60000 e
//	 *  multiplicado por 100 para ter o valor em percentual. Exemplo
//	 *  (27000 / 60000) x 100 = 45,0%
//	 *  - BYTES XXIII e XXIV – Endereço do Escravo atual: Bateria número 27 do
//	 *  banco 2
//	 *  - BYTES de XXV a XXX – Não usados
//	 */
//	out->errcode = msg->err_code;
//	/* Tensao de bateria */
//	deserial.buf[0] = msg->data[0];
//	deserial.buf[1] = msg->data[1];
//	out->vbat = deserial.val;
//	/* Temperatura interna */
//	deserial.buf[0] = msg->data[2];
//	deserial.buf[1] = msg->data[3];
//	out->itemp = deserial.val;
//	/* Temperatura externa */
//	deserial.buf[0] = msg->data[4];
//	deserial.buf[1] = msg->data[5];
//	out->etemp = deserial.val;
//	/* Tensao fonte */
//	deserial.buf[0] = msg->data[6];
//	deserial.buf[1] = msg->data[7];
//	out->vsource = deserial.val;
//	/* Versao de hardware */
//	out->hw_ver = msg->data[8];
//	/* Versao de firmware */
//	out->fw_ver = msg->data[9];
//	/* Offset tensao de bateria */
//	deserial.buf[0] = msg->data[10];
//	deserial.buf[1] = msg->data[11];
//	out->vbat_off = deserial.val;
//	/* Offset corrente de bateria */
//	deserial.buf[0] = msg->data[12];
//	deserial.buf[1] = msg->data[13];
//	out->ibat_off = deserial.val;
//	/* Tensao de referencia */
//	deserial.buf[0] = msg->data[14];
//	deserial.buf[1] = msg->data[15];
//	out->vref = deserial.val;
//	/* Duty cycle atual */
//	deserial.buf[0] = msg->data[16];
//	deserial.buf[1] = msg->data[17];
//	out->duty_cycle = deserial.val;
//	/* Endereco da leitura */
//	out->addr_bank = msg->data[18];
//	out->addr_batt = msg->data[19];
//
//	return 0;
//}

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

//static int prot_creat_impedance_request(ProtocolRequest_t *request,
//		Protocol_ImpedanceCmd_InputVars *in) {
//
//	/*
//	 * Preenchendo informacoes dos campos iniciais do quadro
//	 */
//	request->start_frame = PROTOCOL_START_OF_FRAME;
//	request->addr_bank = in->addr_bank;
//	request->addr_batt = in->addr_batt;
//	request->command = PROTOCOL_IMPEDANCE_COMMAND;
//
//	/*
//	 * Campo de dados e enviado sem informacoes, contendo o valor 0 em tudo
//	 */
//	memset(request->data,0,PROTOTOL_FRAME_REQUEST_DATA_LEN);
//
//	/*
//	 * Calculando checksum
//	 */
//	//request->chksum = prot_calc_checksum((uint16_t *)
//	//		(((uint8_t *)request)+2), 14);
//
//	return 0;
//}

static int prot_check_extract_impedance_response8(uint8_t * data,
                            Protocol_ImpedanceCmd_OutputVars *out)
{
    int err = 0;
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 28);

    if(markedChksum != chksum){
        LOG("Invalid checksum, got: %d expected: %d\n", markedChksum, chksum);
        return -1;
    }

    uint16_t cmd = bytes_to_u16(data[1], data[0]);
    if(cmd != PROTOCOL_IMPEDANCE_COMMAND){
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

//static int prot_check_extract_impedance_response(ProtocolResponse_t *msg,
//		Protocol_ImpedanceCmd_OutputVars *out) {
//	union conv_int {
//		uint32_t val;
//		uint8_t buf[4];
//	};
//	int err = 0;
//	union conv_int deserial;
//
//	/*
//	 * Primeiro passo e checar se o quadro recebido esta correto.
//	 */
//	err = prot_verify_checksum(msg);
//	if (err == 0) {
//		return -1;
//	}
//
//	/*
//	 * Segundo passo e checar se a resposta corresponde ao comando enviado
//	 */
//	if (msg->command != PROTOCOL_IMPEDANCE_COMMAND) {
//		return -2;
//	}
//
//	/*
//	 * A partir de um quadro valido, extrair os parametros de resposta. De
//	 * acordo com a documentacao, o campo data esta organizado em words,
//	 * contendo as seguintes informacoes:
//	 * - BYTES V, VI, VII, VIII – Impedância de Bateria em 32 bits: Esse valor
//	 * deve ser divido por 100 e mostrado na WEB no campo impedância de bateria
//	 * - BYTES IX, X, XI e XII – Corrente de Bateria em 32 bits: Não usado
//	 * interface WEB
//	 * - BYTES de XIII a XXX – Não usados
//	 */
//	out->errcode = msg->err_code;
//	/* Impedancia da bateria */
//	deserial.buf[0] = msg->data[0];
//	deserial.buf[1] = msg->data[1];
//	deserial.buf[2] = msg->data[2];
//	deserial.buf[3] = msg->data[3];
//	out->impedance = deserial.val;
//	/* Impedancia da bateria */
//	deserial.buf[0] = msg->data[4];
//	deserial.buf[1] = msg->data[5];
//	deserial.buf[2] = msg->data[6];
//	deserial.buf[3] = msg->data[7];
//	out->current = deserial.val;
//
//	return 0;
//}



static int prot_communicate(uint8_t *msg8) 
{
	int err = 0;

	/*
	 * Envia a mensagem pela serial. A serializacao do quadro sera feita
	 * atraves de cast do tipo
	 */
	err = ser_write(ser_instance, msg8 ,PROTOCOL_FRAME_LEN);
	if (err != 0) {
		return -1;
	}

	/*
	 * Aguarda a chegada da mensagem de resposta. A serializacao do quadro sera
	 * feita atraves de cast do tipo
	 */
    struct timeval timeout;
    timeout.tv_sec = PROTOCOL_TIMEOUT_VSEC;
    timeout.tv_usec = PROTOCOL_TIMEOUT_USEC;
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t)*(PROTOCOL_FRAME_LEN+1));
	err = ser_read(ser_instance, data, PROTOCOL_FRAME_LEN, timeout);
	if (err != 0) {
		return -2;
	}
    memcpy(msg8, data, sizeof(uint8_t)*(PROTOCOL_FRAME_LEN+1));
    return 0;
}

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

int prot_read_vars(Protocol_ReadCmd_InputVars *in,
		            Protocol_ReadCmd_OutputVars *out) 
{
	int err = 0;
	/* Constroi a mensagem */
	uint8_t *msg8 = prot_creat_readvar_request8(in); 
	if (err != 0) {
		return -1;
	}

	/* Envia o request e aguarda a resposta */
	err = prot_communicate(msg8); //in place response
	if (err != 0) {
		return -2;
	}

	/* Retorna o resultado */
	err = prot_check_extract_readvar_response8(msg8, out);
	if (err != 0) {
		return -3;
	}

	return 0;
}

int prot_read_impedance(Protocol_ImpedanceCmd_InputVars *in,
		                Protocol_ImpedanceCmd_OutputVars *out) 
{
	int err = 0;
	/* Constroi a mensagem */
    
	uint8_t * msg8 = prot_creat_impedance_request8(in);
	if (err != 0) {
		return -3;
	}

	/* Envia o request e aguarda a resposta */
	err = prot_communicate(msg8); //in place response
	if (err != 0) {
		return -3;
	}

	/* Retorna o resultado */
	err = prot_check_extract_impedance_response8(msg8, out);
	if (err != 0) {
		return -3;
	}

	return 0;
}
