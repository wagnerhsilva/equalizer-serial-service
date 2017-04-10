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

int ser_init(Serial_t *ser_instance, const char *ser_device) {
    int err = 0;

    ser_instance->fd = open(ser_device, O_RDWR | O_NOCTTY);
    if (ser_instance->fd < 0) {
        /* Erro de abertura da serial */
        err = -1;
    }

    return err;
}

int ser_setup(Serial_t *ser_instance, int baud) {
    int err = 0;

    if (ser_instance->fd < 0) {
        return -1;
    }

    /*
     * Por hora so aceita a velocidade de 115200
     */
    if (baud != 115200) {
        return -2;
    }

    err = tcgetattr(ser_instance->fd, &ser_instance->old_termios);
    if (err != 0) {
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
        return -4;
    }

    err = cfsetospeed(&ser_instance->new_termios, B115200);
    if (err != 0) {
        return -5;
    }

    err = tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->new_termios);
    if (err != 0) {
        return -6;
    }

    /*
     * Configura o timeout da recepcao
     * Por hora, hardcoded
     */
    ser_instance->read_timeout.tv_sec = 0;
    ser_instance->read_timeout.tv_usec = 10000;

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
    tcsetattr(ser_instance->fd, TCSANOW, &ser_instance->old_termios);

    /*
     * Fecha a serial
     */
    close(ser_instance->fd);

    return 0;
}

int ser_read(Serial_t *ser_instance, uint8_t *data, int *recv_len) {

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

    rv = select(1, &set, NULL, NULL, &ser_instance->read_timeout);
    if (rv == -1) {
        return -2;
    }
    else if (rv == 0) {
        return -3;
    }
    else {
        *recv_len = read(ser_instance->fd, data, *recv_len);
    }

    return 0;
}

int ser_write(Serial_t *ser_instance, uint8_t *data, int len) {

	int bytesSent = 0;

	if (ser_instance->fd < 0) {
		return -1;
	}

	bytesSent = write(ser_instance->fd,data,len);
	if (bytesSent != len) {
		return -2;
	}

	return 0;
}
