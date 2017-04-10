#ifndef PROTOCOL_SERIAL_H
#define PROTOCOL_SERIAL_H

#include <termios.h>
#include <sys/time.h>
#include <stdint.h>

typedef struct {
    int fd;
    struct termios old_termios;
    struct termios new_termios;
    struct timeval read_timeout;
} Serial_t;

int ser_init(Serial_t *ser_instance, const char *ser_device);
int ser_setup(Serial_t *ser_instance, int baud);
int ser_finish(Serial_t *ser_instance);
int ser_write(Serial_t *ser_instance, uint8_t *data, int len);
int ser_read(Serial_t *ser_instance, uint8_t *data, int *recv_len);

#endif
