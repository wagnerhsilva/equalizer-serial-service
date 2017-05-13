#include <defs.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

FILE *fp = NULL;
int DEBUG = 1;
static int firstpass = 1;

const int PROTOCOL_READ_VAR_ARR[2] = {PROTOCOL_READ_VAR_COMMAND_0,
                                     PROTOCOL_READ_VAR_COMMAND_1};

const int PROTOCOL_IMPEDANCE_VAR_ARR[2] = {PROTOCOL_IMPEDANCE_COMMAND_0,
                                          PROTOCOL_IMPEDANCE_COMMAND_1};

int _check(int error, int line, const char *file){
    if(error != 0 && DEBUG){
        LOG("Error file %s on line %d with code %d\n", file, line, error);
        if(fp) fclose(fp);
    }
    return error;
}

static float saturate(float edge0, float edge1, float x){
    return (x < edge0 ? edge0 : (edge1 < x ? edge1 : x));
}

unsigned short _compressFloat(float a){
    float z = saturate(0, USHRT_MAX-1, a);
    z = roundf(z);
    return (unsigned short)(z);
}

int LOG(const char *format, ...){
    int done = -1;  
    if(DEBUG){
        if(!fp){
            fp = fopen(DEBUG_FILE, "a+");
        }
        if(firstpass == 1){
            time_t rawtime;
            struct tm * timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            char buffer[256];
            strcat(buffer, "---[Invoked at: ");
            int s = strlen(buffer);
            strftime(&buffer[s], 100, "%Y-%m-%d %H:%M:%S", timeinfo);
            strcat(buffer, "]---\n");
            firstpass = 0;
            fwrite(buffer, strlen(buffer), 1, fp);
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
