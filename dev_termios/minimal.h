#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#define DEVICE "/dev/ttyS0"

int    terminal_descriptor = -1;
static struct termios old_termios;
static struct termios new_termios;

void terminal_done(void){
    if(terminal_descriptor == -1)
        tcsetattr(terminal_descriptor, TCSANOW, &old_termios);
}


int terminal_init(void){
    terminal_descriptor = open(DEVICE, O_RDWR | O_NOCTTY);
    if(terminal_descriptor < 0){
        printf("Error\n");
        exit(0);
    }

    if(tcgetattr(terminal_descriptor, &old_termios) != 0){
        printf("error tcgetattr\n");
        exit(0);
    }
    
    memset(&new_termios, 0, sizeof(new_termios));
    new_termios.c_iflag = IGNPAR;
    new_termios.c_oflag = 0;
    new_termios.c_cflag = CS8 | CREAD | CLOCAL | HUPCL;
    new_termios.c_lflag = 0;
    new_termios.c_cc[VINTR]    = 0;
    new_termios.c_cc[VQUIT]    = 0;
    new_termios.c_cc[VERASE]   = 0;
    new_termios.c_cc[VKILL]    = 0;
    new_termios.c_cc[VEOF]     = 4;
    new_termios.c_cc[VTIME]    = 0;
    new_termios.c_cc[VMIN]     = 1;
    new_termios.c_cc[VSWTC]    = 0;
    new_termios.c_cc[VSTART]   = 0;
    new_termios.c_cc[VSTOP]    = 0;
    new_termios.c_cc[VSUSP]    = 0;
    new_termios.c_cc[VEOL]     = 0;
    new_termios.c_cc[VREPRINT] = 0;
    new_termios.c_cc[VDISCARD] = 0;
    new_termios.c_cc[VWERASE]  = 0;
    new_termios.c_cc[VLNEXT]   = 0;
    new_termios.c_cc[VEOL2]    = 0;

    if (cfsetispeed(&new_termios, B115200) != 0) {
        fprintf(stderr, "cfsetispeed(&new_termios, B115200) failed: %s\n", strerror(errno));
        return 1;
    }
    if (cfsetospeed(&new_termios, B115200) != 0) {
        fprintf(stderr, "cfsetospeed(&new_termios, B115200) failed: %s\n", strerror(errno));
        return 1;
    }
    if (tcsetattr(terminal_descriptor, TCSANOW, &new_termios) != 0) {
        fprintf(stderr, "tcsetattr(fd, TCSANOW, &new_termios) failed: %s\n", strerror(errno));
        return 1;
    }
}


void restore(void){
    tcsetattr(terminal_descriptor, TCSANOW, &old_termios);
}
