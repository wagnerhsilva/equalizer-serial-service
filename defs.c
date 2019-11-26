#include <defs.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#define OUTPUT_CONSOLE		0
#define MS_TO_MONTH_CONST	3.80517e-7
#define MS_TO_MINUTES_CONST	60000

FILE *fp = NULL;
FILE *ext_fp = NULL;
int DEBUG = 1;
int EXT_PR = 0;

Tendence_Configs_t  TendenceOpts = {0};
Idioma_t            idioma = { 0, "pt-br" };

static int firstpass = 1;
static int ext_first = 1;

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

int EXT_PRINT(const char *format, ...){
  int done = -1;
  if(EXT_PR){
    if(!ext_fp){
        ext_fp = fopen(EXT_PRINT_FILE, "w+");
    }
    if(ext_first == 1){
        time_t rawtime;
        struct tm * timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char buffer[256];
        strcat(buffer,"SOFTWARE BASICO V. " SOFTWARE_VERSION);
        strcat(buffer, "---[Invoked at: ");
        int s = strlen(buffer);
        strftime(&buffer[s], 100, "%Y-%m-%d %H:%M:%S", timeinfo);
        strcat(buffer, "]---\n");
        ext_first = 0;
        fwrite(buffer, strlen(buffer), 1, ext_fp);
    }
    va_list arg;
    va_start(arg, format);
    done = vfprintf(ext_fp, format, arg);
    va_end(arg);
    fflush(ext_fp);
  }
  return done;
}

int LOG(const char *format, ...){
    int done = -1;
    int s = 0;
    va_list arg;
    char timestamp[80];
    char obuffer[256] = {0};
    char buffer[256] = {0};
    time_t rawtime;
    struct tm * timeinfo;

    memset(timestamp,0,sizeof(timestamp));
    
    if(DEBUG){
        if(!fp){
            fp = fopen(DEBUG_FILE, "a+");
        }
        if(firstpass == 1){
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strcat(buffer,"SOFTWARE BASICO V. " SOFTWARE_VERSION);
            strcat(buffer, "---[Invoked at: ");
            s = strlen(buffer);
            strftime(&buffer[s], 100, "%Y-%m-%d %H:%M:%S", timeinfo);
            strcat(buffer, "]---\n");
            firstpass = 0;
            fwrite(buffer, strlen(buffer), 1, fp);
            if(OUTPUT_CONSOLE){
                printf("%s", buffer);
            }
        }
        memset(buffer,0,sizeof(buffer));
        strcpy(buffer," - ");
        /* Captura o timestamp */
        GetTimestampString(timestamp,sizeof(timestamp));
        /* Cria o buffer formatado para salvar em arquivo */
        va_start(arg, format);
        done = vsnprintf(obuffer, 256, format, arg);
        va_end(arg);
        /* Salva buffers em arquivo */
        fwrite(timestamp, strlen(timestamp), 1, fp);
        fwrite(buffer, strlen(buffer), 1, fp);
        fwrite(obuffer, strlen(obuffer), 1, fp);
        fflush(fp);
        if(OUTPUT_CONSOLE){
            printf("%s%s%s", timestamp, buffer, obuffer);
            fflush(stdout);
        }
    }
    return done;
}

char * toStrHexa(unsigned char *data, int len) {
    char *buffer = (char *)malloc(sizeof(char)*(2*len + 1));
    int i = 0;
    for(i = 0; i < len; i++){
        sprintf(buffer + (i * 2), "%02x", data[i]);
    }

    buffer[2*len] = 0;
    return buffer;
}

void GetTimeString(char *Buffer, size_t size, const char *Format, time_t value){
    struct tm * timeinfo;
	timeinfo = localtime(&value);
	strftime (Buffer,size, Format,timeinfo);
}

time_t GetCurrentTime(void){
    time_t rawtime;
    time(&rawtime);
    return rawtime;
}

void GetTimestampString(char *Buffer, int bufLen) {
    time_t ts = GetCurrentTime();

    return GetTimeString(Buffer, bufLen,"%Y/%m/%d-%H:%M:%S", ts);
}

time_t GetTimeFromString(const char *Format, char *Buffer){
    struct tm tm;
    strptime(Buffer, Format, &tm);
    return mktime(&tm);
}

int GetDifferenceInMonths(char *date0, char *date1) {

    int d0_y = 0;
    int d0_m = 0;
    int d0_d = 0;
    int d1_y = 0;
    int d1_m = 0;
    int d1_d = 0;

    if (sscanf(date0,"%d/%d/%d",&d0_d,&d0_m,&d0_y) != 3) {
        LOG("Erro processamento Date0\n");
        return -1;
    }

    if (sscanf(date1,"%d/%d/%d",&d1_d,&d1_m,&d1_y) != 3) {
        LOG("Erro processamento Date1\n");
        return -1;
    }

    if ((d0_y - d1_y) > 0) {
        return (d0_y - d1_y)*12 + d1_m;
    } else if ((d0_y - d1_y) == 0) {
        /* Caso a diferenca de mes seja um, e preciso
         * analisar os dias, pois o resultado pode ser 
         * zero */
        if ((d0_m - d1_m) == 1) {
            if (d0_d < d1_d) {
                return 0;
            } else {
                return 1;
            }
        } else {
            return (d0_m - d1_m);
        }
    } else {
        return -1;
    }
}

/*
 * Computes Date0 - Date1 return the result in days
*/
int GetDifferenceInDays(char * Date0, char *Date1){
    return 0;
}

void sleep_ms(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int is_file(const char *name){
    return access(name, F_OK) != -1;
}
