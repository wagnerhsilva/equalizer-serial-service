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
    //uint8_t *request8 = 0;
    //request8 = (uint8_t*)malloc(sizeof(uint8_t)*PROTOCOL_FRAME_LEN);
    static uint8_t request8[PROTOCOL_FRAME_LEN] = { 0 };
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
    request8[pointer++] = u16_MSB(in->index);
    request8[pointer++] = u16_LSB(in->index);
    uint16_t chksum = prot_calc_checksum(&request8[2], 28);
    request8[30] = u16_MSB(chksum);
    request8[31] = u16_LSB(chksum);
    return request8;
}

static int prot_check_extract_readvar_response8(uint8_t * data,Protocol_ReadCmd_OutputVars *out)
{
    int err = 0;
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 30);
    
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
                            Protocol_ImpedanceCmd_OutputVars *out)
{
    int err = 0;
    uint16_t markedChksum = bytes_to_u16(data[31], data[30]);
    uint16_t chksum = prot_calc_checksum(&data[0], 30);

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
        free(data);
		return -2;
	}
    memcpy(msg8, data, sizeof(uint8_t)*(PROTOCOL_FRAME_LEN+1));
    free(data);
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
