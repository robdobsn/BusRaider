
#pragma once

#include "globaldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W32(addr, val) (*(volatile unsigned *)(addr)) = (val)
#define R32(addr) (*(volatile unsigned *)(addr))

extern void PUTW32(unsigned int addr, unsigned int val);
extern unsigned int GETW32(unsigned int addr);

extern int timer_isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);

extern void system_init();

extern char* strncpy(char* dest, const char* src, size_t num);
extern int strncmp(const char* str1, const char* str2, size_t num);
extern int stricmp(const char* str1, const char* str2);
extern int strnicmp(const char* str1, const char* str2, size_t num);
extern char *strstr(const char* string, const char* substring);
extern char rdtolower(char c);
extern char rdisupper (unsigned char c);
extern int rdisdigit(int c);
extern char rdisspace (unsigned char c);
extern int rdisalpha(int c);
extern void strrev(unsigned char *str);
extern int rditoa(int num, unsigned char* str, int len, int base);

extern bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen);

extern long strtol(const char *nptr, char **endptr, register int base);

extern void * memcpy (void *dest, const void *src, size_t len);
extern void *memset (void* pBuffer, int nValue, size_t nLength);

// extern void __aeabi_unwind_cpp_pr0(void);
// extern void __cxa_end_cleanup(void);

// Error handler for pure virtual functions
extern void __cxa_pure_virtual();

#ifdef __cplusplus
}
#endif
