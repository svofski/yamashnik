#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "diags.h"

LOGLEVEL LogLevel = LOGLEVEL_INFO;

static void log(LOGLEVEL threshold, const char* fmt, va_list ap) {
	if (LogLevel >= threshold) {
		vprintf(fmt, ap);
		fflush(stdout);
	}
}

void verbose(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	log(LOGLEVEL_VERBOSE, fmt, ap);
	va_end(ap);
}

void morbose(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	log(LOGLEVEL_MORBOSE, fmt, ap);
	va_end(ap);
}

void info(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	log(LOGLEVEL_INFO, fmt, ap);
	va_end(ap);
}

void eggog(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	log(LOGLEVEL_ZERO, fmt, ap);
	va_end(ap);
	exit(1);
}

void dumpline(printfunc p, const uint8_t* data, int ofs, int length) {
	p("%04x  ", ofs);
	for (int i = 0; i < length; i++) {
		p("%02x%c", data[ofs+i], i == 7 ? '-':' ');
	}

	for (int i = 0; i < 16 - length; i++) p("   ");

	p(" ");
	for (int i = 0; i < length; i++) {
		p("%c", (data[ofs+i] >= 32 && data[ofs+i] < 128) ? 
			data[ofs+i] : '.');
	}
	p("\n");
}

void dump(printfunc p, const uint8_t* data, int length) {
	int ofs = 0;
	for (int line = 0; line < length / 16; line++) {
		dumpline(p, data, ofs, 16);
		ofs += 16;
	}
	dumpline(p, data, ofs, length % 16);
}

