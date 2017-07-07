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
#include <errno.h>

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
        LOG(SERIAL_LOG "Unable to acquire terminal control\n");
        return -3;
    }

    memset(&ser_instance->new_termios, 0, sizeof(struct termios));

    ser_instance->new_termios.c_cflag &= ~PARENB; 					//No Parity
    ser_instance->new_termios.c_cflag &= ~CSTOPB; 					//Stop Bits = 1
    ser_instance->new_termios.c_cflag &= ~CSIZE; 					//Clear Mask
    ser_instance->new_termios.c_cflag |=  CS8; 						//Set Data bits = 8
    ser_instance->new_termios.c_cflag &= ~CRTSCTS; 					//No RTS and CTS
    ser_instance->new_termios.c_cflag |= CREAD|CLOCAL;				//Turn ON Receiver
    ser_instance->new_termios.c_iflag &= ~(IXON | IXOFF | IXANY); 	//No flow control
    ser_instance->new_termios.c_iflag &= ~(ICANON|ECHO|ECHOE|ISIG);	//No flow control
    ser_instance->new_termios.c_oflag &= ~OPOST; 					//No Output Processing
    // Minicom Flags
    ser_instance->new_termios.c_cflag &= ~(HUPCL);					//AFTER
    ser_instance->new_termios.c_iflag &= ~(ICRNL);                 	//AFTER
    ser_instance->new_termios.c_oflag &= ~(ONLCR);                 	//AFTER
    ser_instance->new_termios.c_lflag &= ~(ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE); //AFTER
    // Minicom Flags
    ser_instance->new_termios.c_cc[VMIN]=32;						//Read at least 32 chars
    ser_instance->new_termios.c_cc[VTIME]=0;						//Wait indefitly

    err = cfsetispeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        LOG(SERIAL_LOG "Unable to set input baudrate\n");
        return -4;
    }

    err = cfsetospeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        LOG(SERIAL_LOG "Unable to set output baudrate\n");
        return -5;
    }

    err = tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->new_termios);
    if (err != 0) {
        LOG(SERIAL_LOG "Unable to take controll of terminal\n");
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

int ser_read(Serial_t *ser_instance, uint8_t *data, int exp_len) {
    fd_set set;
    int rv;
    int bread;
    int ret = -5;
    struct timeval timeout;

    LOG("Instance: %d\n", ser_instance->fd);
    /* Sanity check */
    if (ser_instance->fd < 0) {
      return -1;
    }
    /*
     * Libera dados antigos
     */
    tcflush(ser_instance->fd,TCIFLUSH);
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
    while (1) {
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
    			break; /* Sai do loop */
    		} else {
    			/* Erro, tenta novamente */
    			LOG(SERIAL_LOG "read ERROR %d\n",bread);
    		}
    	} else if (rv == 0) {
    		LOG(SERIAL_LOG "select TIMEOUT\n");
    		ret = -3;
    		break; /* Sai do loop */
    	} else {
    		LOG(SERIAL_LOG "select ERROR\n");
    		ret = -2;
    		break; /* Sai do loop */
    	}
    }

    return ret;
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

int ser_setReadRetries(Serial_t *ser_instance, unsigned int retries) {

	ser_instance->retries = retries;

	LOG(SERIAL_LOG "Set new read retries = %d\n",ser_instance->retries);

	return 0;
}
