#ifndef DEFS_H
#define DEFS_H
#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#define SOFTWARE_VERSION					"1.11"

#define DEBUG_FILE "debug.txt"
/******************************************/
#define EXT_PRINT_FILE "debug_print.txt"
/*****************************************/
#define CHECK(r) _check((r), __LINE__, __FILE__)
#define PROTOCOL_FRAME_LEN                  32
#define PROTOCOL_FRAME_LEN_SHORT            (PROTOCOL_FRAME_LEN / 2)
#define PROTOTOL_FRAME_REQUEST_DATA_LEN     24
#define PROTOCOL_FRAME_RESPONSE_DATA_LEN    26
//slave should sent a begining of message 2byte package, but whatever
#define PROTOCOL_READ_VAR_COMMAND_0         0x10
#define PROTOCOL_READ_VAR_COMMAND_1         0xC0
#define PROTOCOL_IMPEDANCE_COMMAND_0        0x10
#define PROTOCOL_IMPEDANCE_COMMAND_1        0xB0
#define PROTOCOL_TIMEOUT_VSEC               10
#define PROTOCOL_TIMEOUT_USEC               1000
#define DATABASE_JOURNAL_MODE               "PRAGMA journal_mode=WAL"
#define DATABASE_BUSY_TIMEOUT               "PRAGMA busy_timeout=10000"
#define DATABASE_TRANSACTION_BEG            "BEGIN TRANSACTION"
#define DATABASE_TRANSACTION_END            "END TRANSACTION"
#define MAX_STRING_LEN                      256

#define DEFAULT_DB_PATH                     "../equalizer-api/equalizer-api/equalizerdb"
#define UPDATED_FILE 						"../equalizer-api/equalizer-api/updated.txt"
#define bytes_to_u16(LSB, MSB) (LSB | MSB << 8)
#define bytes_to_u32(MSB0,MSB1,MSB2,LSB) ((MSB0 << 24) | (MSB1 << 16) \
                                         |(MSB2 << 8)  | (LSB))
#define u16_LSB(u16) (u16 & 0xFF)
#define u16_MSB(u16) ((u16 >> 8) & 0xFF)

typedef struct{
	float average;
	unsigned int bus_sum;
}StringAvg_t;

/*
 * Structure to be saved inside Tendencias table
 * per battery
*/
typedef struct{
	int 			MesaureInteraction; //what measure is this (0, 1, ...)
	int				Temperature; //temperature of the read
	unsigned int 	Impendance; //impedance of the read
	unsigned int 	Battery; //which battery is this (index)
	unsigned int 	String; //which sttring is this (index)
	time_t 			CurrentTime; //time of definition
}Tendence_t;

typedef struct{
	int    IsConfigured;
	time_t InstallTime;
	time_t LastWrite;
	int    LastIteration; //last iteraction to be written
	int    PeriodInitial; //this generates Zero Date
	int    PeriodConstant; //this generates Following Dates
	float  ImpeMin;
	float  ImpeMax;
	float  TempMin;
	float  TempMax;
	int	   HasWrites;
	int	   testMode;
}Tendence_Configs_t;

extern Tendence_Configs_t TendenceOpts;

typedef struct{
	int i1, i2, i3;
}int3;

#define autofree __attribute__((cleanup(free_stack)))

__attribute__ ((always_inline))
inline void free_stack(void *ptr) {
	printf("Freeing %s\n", (const char *)(ptr));
    free(*(void **) ptr);
}

//defined in defs.c
extern const int PROTOCOL_READ_VAR_ARR[2];
extern const int PROTOCOL_IMPEDANCE_VAR_ARR[2];

enum bool{
    false = 0, true = !false
};

typedef enum bool bool;

time_t GetCurrentTime(void);
time_t GetTimeFromString(const char *Format, char *Buffer);
double GetDifferenceInMonths(time_t Date0, time_t Date1); //Date0 - Date1
void GetTimeString(char * Buffer, size_t size, const char *Format, time_t value);

int LOG(const char *format, ...);
int EXT_PRINT(const char *format, ...);
int _check(int error, int line, const char *file);
char * toStrHexa(unsigned char * data, int len);
unsigned short _compressFloat(float a);
void sleep_ms(int milliseconds);
int is_file(const char *name);

#endif
