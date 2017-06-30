#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include "serial.h"
#include <defs.h>

#define SERIAL_LOG "SERIAL:"

int ser_init(Serial_t *ser_instance, const char *ser_device) {
    int err = 0;

    ser_instance->fd = open(ser_device, O_RDWR | O_NOCTTY);
    if (ser_instance->fd < 0) {
        LOG(SERIAL_LOG "Unnable to open serial, maybe you don't have permision or the port is incorrect?\n");
        /* Erro de abertura da serial */
        err = -1;
    }

    return err;
}

int ser_setup(Serial_t *ser_instance, int baud) {
    int err = 0;

    if (ser_instance->fd < 0) {
        LOG("Invalid serial port\n");
        return -1;
    }

    /*
     * Por hora so aceita a velocidade de 115200
     */
    if (baud != 115200) {
        LOG(SERIAL_LOG "We can only handle baudrate as 115200\n");
        return -2;
    }

    err = tcgetattr(ser_instance->fd, &ser_instance->old_termios);
    if (err != 0) {
        LOG(SERIAL_LOG "Unnable to acquire terminal controll\n");
        return -3;
    }

    memset(&ser_instance->new_termios, 0, sizeof(struct termios));

    ser_instance->new_termios.c_iflag = IGNPAR;
    ser_instance->new_termios.c_oflag = 0;
    ser_instance->new_termios.c_cflag = CS8 | CREAD | CLOCAL | HUPCL;
    ser_instance->new_termios.c_lflag = 0;
    ser_instance->new_termios.c_cc[VINTR]    = 0;
    ser_instance->new_termios.c_cc[VQUIT]    = 0;
    ser_instance->new_termios.c_cc[VERASE]   = 0;
    ser_instance->new_termios.c_cc[VKILL]    = 0;
    ser_instance->new_termios.c_cc[VEOF]     = 4;
    ser_instance->new_termios.c_cc[VTIME]    = 0;
    ser_instance->new_termios.c_cc[VMIN]     = 1;
    ser_instance->new_termios.c_cc[VSWTC]    = 0;
    ser_instance->new_termios.c_cc[VSTART]   = 0;
    ser_instance->new_termios.c_cc[VSTOP]    = 0;
    ser_instance->new_termios.c_cc[VSUSP]    = 0;
    ser_instance->new_termios.c_cc[VEOL]     = 0;
    ser_instance->new_termios.c_cc[VREPRINT] = 0;
    ser_instance->new_termios.c_cc[VDISCARD] = 0;
    ser_instance->new_termios.c_cc[VWERASE]  = 0;
    ser_instance->new_termios.c_cc[VLNEXT]   = 0;
    ser_instance->new_termios.c_cc[VEOL2]    = 0;

    err = cfsetispeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        LOG(SERIAL_LOG "Unnable to set input baudrate\n");
        return -4;
    }

    err = cfsetospeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        LOG(SERIAL_LOG "Unnable to set output baudrate\n");
        return -5;
    }

    err = tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->new_termios);
    if (err != 0) {
        LOG(SERIAL_LOG "Unnable to take controll of terminal\n");
        return -6;
    }

    /* Configura o timeout padrao */
    ser_instance->read_timeout = PROTOCOL_TIMEOUT_VSEC;

    return 0;
}

int ser_finish(Serial_t *ser_instance) {

    /*
     * Caso a serial nao tenha sido aberta, nao
     * tem o que fazer
     */
    if (ser_instance->fd < 0) {
        return 0;
    }

    /*
     * Retorna os parametros iniciais da serial
     */
    close(ser_instance->fd);
    tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->old_termios);

    /*
     * Fecha a serial
     */
    //close(ser_instance->fd);

    return 0;
}


//static int ser_match_command(uint8_t *buffer, int bufferSize){
//    int response = 0;
//    int byte0 = buffer[0];
//    int byte1 = buffer[1];
//
//    if((byte0 == PROTOCOL_READ_VAR_ARR[0] &&
//       byte1 == PROTOCOL_READ_VAR_ARR[1])||
//      (byte0 == PROTOCOL_IMPEDANCE_VAR_ARR[0] &&
//       byte1 == PROTOCOL_IMPEDANCE_VAR_ARR[1]))
//    {
//       response = 1;
//    }
//
//    return response;
//}

int ser_read(Serial_t *ser_instance, uint8_t *data, int exp_len) {
    fd_set set;
    int rv;
    int bread;
    int ret = -5;
    struct timeval timeout;
    
    /* Sanity check */
    if (ser_instance->fd < 0) {
        return -1;
    }
    /*
     * Preparando o conjunto para o select()
     * No caso, estamos assumindo somente uma serial usada
     * na execucao do software. Assim, o valor do file descriptor
     * sera sempre 0.
     */
    FD_ZERO(&set);
    FD_SET(ser_instance->fd,&set);
    timeout.tv_sec = ser_instance->read_timeout;
    timeout.tv_usec = 0;

    /* Usa o select() para verificar a disponibilidade de dados */
    rv = select(ser_instance->fd + 1, &set, NULL, NULL, &timeout);
    if (rv == 1) {
    	/* Dados disponiveis */
    	bread = read(ser_instance->fd, data, exp_len);
    	if (bread == exp_len) {
    		/* Sucesso */
    		char *buffer = toStrHexa(data, bread);
    		LOG(SERIAL_LOG "<== %s\n", buffer);
    		free(buffer);
    		ret = 0;
    	} else {
    		/* Erro */
    		LOG(SERIAL_LOG "read ERROR\n");
    		ret = -4;
    	}
    } else if (rv == 0) {
    	LOG(SERIAL_LOG "read TIMEOUT\n");
    	ret = -3;
    } else {
    	LOG(SERIAL_LOG "select ERROR\n");
    	ret = -2;
    }

    return ret;

//    int bytes_read = 0;
//    int bytes_expected = exp_len;
//    int foundPackage = 0;
//
//
//    while(bytes_read < bytes_expected){
//    	struct timeval timeout;
//    	timeout.tv_sec = 10;
//    	timeout.tv_usec = 100;
//    	LOG(SERIAL_LOG "read timeout: sec=%d usec=%d\n",timeout.tv_sec,timeout.tv_usec);
//    	/* Checa a presenca de dados */
//        rv = select(ser_instance->fd + 1, &set, NULL, NULL, &timeout);
//        if(rv == -1){
//            LOG(SERIAL_LOG "error select()\n");
//            return -2;
//        }else if(rv == 0){
//            LOG(SERIAL_LOG "read TIMEOUT\n");
//            return -3;
//        }else{
//            int bread = read(ser_instance->fd, (void *)&(data[bytes_read]), bytes_expected);
//            bytes_read += bread;
//            data[bytes_read] = 0;
//            if(bytes_read == 1)
//            	continue; //we need to wait for the header
//            if(!foundPackage){
//                int isMatch = ser_match_command(data, bytes_read);
//                if(!isMatch){
//                    bytes_read = 0;
//                    data[0] = 0;
//                }else{
//                    foundPackage = 1;
//                }
//            }
//        }
//    }
//
//    char *buffer = toStrHexa(data, bytes_read);
//    LOG(SERIAL_LOG "<== %s\n", buffer);
//    free(buffer);
//
//    return 0;
//
}

int ser_write(Serial_t *ser_instance, uint8_t *data, int len) {
	int bytesSent = 0;
	if (ser_instance->fd < 0) {
		return -1;
	}
    
    char *buffer = toStrHexa(data, len);
    LOG(SERIAL_LOG "==> %s\n", buffer);
    free(buffer);
	bytesSent = write(ser_instance->fd,data,len);
	if (bytesSent != len) {
		return -2;
	}

	return 0;
}

int ser_setReadTimeout(Serial_t *ser_instance, unsigned int timeout) {

    ser_instance->read_timeout = timeout;

    LOG(SERIAL_LOG "Set new timeout: %d\n",ser_instance->read_timeout);

    return 0;
}

