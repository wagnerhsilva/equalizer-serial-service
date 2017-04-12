#include <minimal.h>
#include <sys/select.h>
#include <sys/time.h>

void main(void){
    terminal_init();
    
    fd_set set;
    FD_ZERO(&set);
    FD_SET(terminal_descriptor, &set);

    char *s = "3";
    write(terminal_descriptor, s, 1);
    
    struct timeval read_timeout;
    read_timeout.tv_sec = 10;
    read_timeout.tv_usec = 0;    
    
    int bread = 0;
    int expected = 32;
    int found = 0;
    char *data = (char *)malloc(sizeof(char)*125);
    while(bread < expected){
        int rv = select(terminal_descriptor + 1, &set, NULL, NULL, &read_timeout);
        if(rv == -1){
            printf("select error");
            break;
        }
        else if(rv == 0){
            printf("select timeout");
            break;
        }else{
            int len = read(terminal_descriptor, (void *)&(data[bread]), expected);
            bread += len;
        }
        if(bread >= expected){ 
            found = 1;
             break;
        }
    }
    
    if(found == 1){
        data[bread] = 0;
        int i = 0;
        char buffer[150];
        for(i = 0; i < bread; i++){
            sprintf(buffer + (i * 2), "%02x", data[i]);
        }
        buffer[2*bread] = 0;
        printf("Got: %s\n", buffer);
    }
    restore();   
}
