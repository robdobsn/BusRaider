
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char rdtolower(char c);
extern char rdisupper (unsigned char c);
extern char rdtoupper(char c);
extern char rdislower (unsigned char c);
extern int rdisdigit(int c);
extern char rdisspace (unsigned char c);
extern int rdisalpha(int c);
extern void rdstrrev(unsigned char *str);
extern int rditoa(int num, unsigned char* str, int len, int base);

extern bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen);
extern int jsonGetArrayLen(const char* jsonStr);
extern bool jsonGetArrayElem(uint32_t arrayIdx, const char* jsonStr, char* pOutStr, int outStrMaxLen);
extern void jsonEscape(const char* inStr, char* outStr, int maxLen);

#ifdef __cplusplus
}
#endif
