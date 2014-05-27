#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _loglevel {
	LOGLEVEL_ZERO = 0, 
	LOGLEVEL_INFO = 1, 
	LOGLEVEL_VERBOSE = 4, 
	LOGLEVEL_MORBOSE = 5
} LOGLEVEL;

extern LOGLEVEL LogLevel;

typedef void (*printfunc)(const char* fmt, ...);

void eggog(const char* fmt, ...);
void info(const char* fmt, ...);
void verbose(const char* fmt, ...);
void morbose(const char* fmt, ...);

void dump(printfunc p, const uint8_t* data, int length);

#ifdef __cplusplus
}
#endif
