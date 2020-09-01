// Bus Raider
// Low-level library
// Rob Dobson 2019

#pragma once

#include <stddef.h>
#include <linux/types.h>
#include <circle/stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long micros();
extern unsigned long millis();
extern void microsDelay(unsigned long us);
extern int isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
unsigned long timeToTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
extern void heapInit();

extern size_t strlcpy(char * dst, const char * src, size_t dsize);
extern size_t strlcat(char * dst, const char * src, size_t maxlen);
extern size_t strnlen(const char *s, size_t maxlen);
int sprintf (char *buf, const char *fmt, ...);
int snprintf (char *buf, size_t size, const char *fmt, ...);
int vsnprintf (char *buf, size_t size, const char *fmt, va_list var);

// extern int strcasecmp (const char *s1, const char *s2);
// extern long strtol(const char *nptr, char **endptr, register int base);
// extern unsigned long strtoul(const char *nptr, char **endptr, register int base);

#ifdef __cplusplus
}
#endif