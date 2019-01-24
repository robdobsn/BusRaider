
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char rdtolower(char c);
extern char rdisupper (unsigned char c);
extern int rdisdigit(int c);
extern char rdisspace (unsigned char c);
extern int rdisalpha(int c);
extern void rdstrrev(unsigned char *str);
extern int rditoa(int num, unsigned char* str, int len, int base);

extern bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen);

// extern void __aeabi_unwind_cpp_pr0(void);
// extern void __cxa_end_cleanup(void);

// Error handler for pure virtual functions
extern void __cxa_pure_virtual();

#ifdef __cplusplus
}
#endif
