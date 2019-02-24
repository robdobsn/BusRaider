
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

#ifdef __cplusplus
}
#endif
