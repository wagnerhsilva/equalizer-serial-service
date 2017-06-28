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

int ser_init(Serial_t *ser_instance, const char *ser_device) {
    int err = 0;

    ser_instance->fd = open(ser_device, O_RDWR | O_NOCTTY);
    if (ser_instance->fd < 0) {
        LOG("Unnable to open serial, maybe you don't have permision or the port is incorrect?\n");
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
        LOG("We can only handle baudrate as 115200\n");
        return -2;
    }

    err = tcgetattr(ser_instance->fd, &ser_instance->old_termios);
    if (err != 0) {
        LOG("Unnable to acquire terminal controll\n");
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
        LOG("Unnable to set input baudrate\n");
        return -4;
    }

    err = cfsetospeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        LOG("Unnable to set output baudrate\n");
        return -5;
    }

    err = tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->new_termios);
    if (err != 0) {
        LOG("Unnable to take controll of terminal\n");
        return -6;
    }

    /* Configura o timeout padrao */
    ser_instance->read_timeout.tv_sec = PROTOCOL_TIMEOUT_VSEC;
    ser_instance->read_timeout.tv_usec = PROTOCOL_TIMEOUT_USEC;

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


static int ser_match_command(uint8_t *buffer, int bufferSize){
    int response = 0;
    int i = 0;
    int byte0 = buffer[0];
    int byte1 = buffer[1];

    if((byte0 == PROTOCOL_READ_VAR_ARR[0] &&
       byte1 == PROTOCOL_READ_VAR_ARR[1])||
      (byte0 == PROTOCOL_IMPEDANCE_VAR_ARR[0] &&
       byte1 == PROTOCOL_IMPEDANCE_VAR_ARR[1]))
    {
       response = 1;
    }     

    return response;
}

int ser_read(Serial_t *ser_instance, uint8_t *data, int exp_len) {
    fd_set set;
    int rv;
    
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
        
    int bytes_read = 0;
    int bytes_expected = exp_len;
    int foundPackage = 0;
    int timeoutReceived = 0;

    while(bytes_read < bytes_expected){
        rv = select(ser_instance->fd + 1, &set, NULL, NULL, &(ser_instance->read_timeout));
        if(rv == -1){
            LOG("Unnable to perform select, maybe you didn't run this as super user?\n");
            return -2;
        }else if(rv == 0){
            LOG("Battery timeout, maybe your battery is not connected or the port is not correct?\n");
            return -3;
        }else{
            int bread = read(ser_instance->fd, (void *)&(data[bytes_read]), bytes_expected);
            bytes_read += bread;
            data[bytes_read] = 0;
            if(bytes_read == 1) continue; //we need to wait for the header
            if(!foundPackage){
                int isMatch = ser_match_command(data, bytes_read);
                if(!isMatch){
                    bytes_read = 0;
                    data[0] = 0;
                }else{
                    foundPackage = 1;
                }
            }
        }
    }

    char *buffer = toStrHexa(data, bytes_read);
    //LOG("Got package: %s\n", buffer);
    free(buffer);
    return 0;
   
}

int ser_write(Serial_t *ser_instance, uint8_t *data, int len) {
	int bytesSent = 0;
    int i = 0;
	if (ser_instance->fd < 0) {
		return -1;
	}
    
    char *buffer = toStrHexa(data, len);
    //LOG("Sending package: %s\n", buffer);
    free(buffer);
	bytesSent = write(ser_instance->fd,data,len);
	if (bytesSent != len) {
		return -2;
	}

	return 0;
}

int ser_setReadTimeout(Serial_t *ser_instance, unsigned int timeout) {
    unsigned int to_sec = timeout / 1000000;
    unsigned int to_usec = timeout % 1000000;

    ser_instance->read_timeout.tv_sec = to_sec;
    ser_instance->read_timeout.tv_usec = to_usec;

    LOG("Set new timeout\n");
    LOG("tv_sec = %d\n",ser_instance->read_timeout.tv_sec);
    LOG("tv_usec = %d\n",ser_instance->read_timeout.tv_usec);

    return 0;
}

