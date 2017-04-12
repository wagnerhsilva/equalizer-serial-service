#include <defs.h>

FILE *fp = NULL;
const int DEBUG = 1;

const int PROTOCOL_READ_VAR_ARR[2] = {PROTOCOL_READ_VAR_COMMAND_0,
                                     PROTOCOL_READ_VAR_COMMAND_1};

const int PROTOCOL_IMPEDANCE_VAR_ARR[2] = {PROTOCOL_IMPEDANCE_COMMAND_0,
                                          PROTOCOL_IMPEDANCE_COMMAND_1};

int _check(int error, int line, const char *file){
    if(error != 0){
        LOG("Error file %s on line %d with code %d\n", file, line, error);
        if(fp) fclose(fp);
    }
    return error;
}

int LOG(const char *format, ...){
    int done = -1;  
    if(DEBUG){
        if(!fp){
            fp = fopen(DEBUG_FILE, "a+");
        }
        va_list arg;
        va_start(arg, format);
        done = vfprintf(fp, format, arg);
        va_end(arg);
        fflush(fp);
    }
    return done;
}

char * toStrHexa(unsigned char *data, int len){
    char *buffer = (char *)malloc(sizeof(char)*(2*len + 1));
    int i = 0;
    for(i = 0; i < len; i++){
        sprintf(buffer + (i * 2), "%02x", data[i]);
    }
    
    buffer[2*len] = 0;
    return buffer;
}
