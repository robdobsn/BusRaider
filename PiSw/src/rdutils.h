
#pragma once

#include "globaldefs.h"

extern int timer_isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);

extern void system_init();

extern char* strncpy(char* dest, const char* src, size_t num);
extern int strncmp(const char* str1, const char* str2, size_t num);

extern bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen);

extern long strtol(const char *nptr, char **endptr, register int base);