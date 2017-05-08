#ifndef DEFS_H
#define DEFS_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#define DEBUG_FILE "debug.txt"
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


//#define DEFAULT_SERIAL_PATH                 "/dev/ttyUSB1" //debug
//#define DEFAULT_SERIAL_PATH                 "/dev/ttyS0" //debug
#define DEFAULT_DB_PATH                     "../equalizer-api/equalizer-api/equalizerdb"
//#define DEFAULT_DB_PATH			      "equalizerdb"
#define bytes_to_u16(LSB, MSB) (LSB | MSB << 8)
#define bytes_to_u32(MSB0,MSB1,MSB2,LSB) ((MSB0 << 24) | (MSB1 << 16) \
                                         |(MSB2 << 8)  | (LSB))
#define u16_LSB(u16) (u16 & 0xFF)
#define u16_MSB(u16) ((u16 >> 8) & 0xFF)

//defined in defs.c
extern const int PROTOCOL_READ_VAR_ARR[2];
extern const int PROTOCOL_IMPEDANCE_VAR_ARR[2];

enum bool{ 
    false = 0, true = !false
};

typedef enum bool bool;

int LOG(const char *format, ...);
int _check(int error, int line, const char *file);
char * toStrHexa(unsigned char * data, int len);
unsigned short _compressFloat(float a);
#endif
