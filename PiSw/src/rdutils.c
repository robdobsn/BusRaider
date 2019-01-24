// Bus Raider
// Rob Dobson 2018

#include "nmalloc.h"
#include "jsmnR.h"
#include "ee_printf.h"
#include <string.h>

// char* strlcpy(char* dest, const char* src, size_t num)
// {
//     while(*src && num--)
//     {
//         *dest++ = *src++;
//     }
//     *dest = '\0';
//     return dest;
// }

// int strncmp(const char* str1, const char* str2, size_t num)
// {
//     if (num == 0)
//         return (0);
//     do {
//         if (*str1 != *str2++)
//             return (*str1 - *(--str2));
//         if (*str1++ == 0)
//             break;
//     } while (--num != 0);
//     return (0);
// }

// int stricmp(const char* str1, const char* str2)
// {
//     do {
//         if (rdtolower(*str1) != rdtolower(*str2++))
//             return (rdtolower(*str1) - rdtolower(*(--str2)));
//         if (*str1++ == 0)
//             break;
//     } while (1);
//     return (0);
// }

// int strnicmp(const char* str1, const char* str2, size_t num)
// {
//     if (num == 0)
//         return (0);
//     do {
//         if (rdtolower(*str1) != rdtolower(*str2++))
//             return (rdtolower(*str1) - rdtolower(*(--str2)));
//         if (*str1++ == 0)
//             break;
//     } while (--num != 0);
//     return (0);
// }

// // strstr from Apple open source
// char *strstr(const char* string, const char* substring)
// {
//     const char *a, *b;

//     /* First scan quickly through the two strings looking for a
//      * single-character match.  When it's found, then compare the
//      * rest of the substring.
//      */

//     b = substring;
//     if (*b == 0) {
//     return (char*)string;
//     }
//     for ( ; *string != 0; string += 1) {
//     if (*string != *b) {
//         continue;
//     }
//     a = string;
//     while (1) {
//         if (*b == 0) {
//         return (char*)string;
//         }
//         if (*a++ != *b++) {
//         break;
//         }
//     }
//     b = substring;
//     }
//     return NULL;
// }

bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen)
{
    // pCmdJson is a JSON string containing the command to execute (amongst other things possibly)
    #define MAX_TOKENS 20
    JSMNR_parser parser;
    jsmnrtok_t tokens[MAX_TOKENS];
    JSMNR_init(&parser);
    int tokenCountRslt = JSMNR_parse(&parser, jsonStr, strlen(jsonStr), tokens, MAX_TOKENS);
    if (tokenCountRslt < 0) {
        LogWrite("rdutils", LOG_DEBUG, "parseJson result %d maxTokens %d jsonLen %d", 
                        tokenCountRslt, MAX_TOKENS, strlen(jsonStr));
        return false;
    }

    // for (int j = 0; j < tokenCountRslt; j++)
    // {
    //     ee_printf("tok %d %d %d %d\n", tokens[j].type, tokens[j].start, tokens[j].end, tokens[j].size);
    // }

    for (int tokIdx = 0; tokIdx < tokenCountRslt; tokIdx++)
    {
        jsmnrtok_t* pTok = tokens + tokIdx;
        if ((pTok->type == JSMNR_STRING) && ((int)strlen(srchKey) == pTok->end - pTok->start) &&
                 (strncmp(jsonStr + pTok->start, srchKey, pTok->end - pTok->start) == 0))
        {
            int stringLen = (pTok+1)->end - (pTok+1)->start;
            if (stringLen >= outStrMaxLen)
                return false;
            strlcpy(pOutStr, jsonStr + (pTok+1)->start, stringLen+1);
            // ee_printf("rdutils_json: srchkey %s value len %d val %s ll %d\n", srchKey, stringLen, pOutStr, strlen(pOutStr));
            return true;
        }
    }
    return false;
}

// #ifndef ULONG_MAX
// #define ULONG_MAX   ((unsigned long)(~0L))      /* 0xFFFFFFFF */
// #endif

// #ifndef LONG_MAX
// #define LONG_MAX    ((long)(ULONG_MAX >> 1))    /* 0x7FFFFFFF */
// #endif

// #ifndef LONG_MIN
// #define LONG_MIN    ((long)(~LONG_MAX))     /* 0x80000000 */
// #endif

char rdisspace (unsigned char c) 
{
  if ( c == ' '
    || c == '\f'
    || c == '\n'
    || c == '\r'
    || c == '\t'
    || c == '\v' ) 
      return 1;

  return 0;
}

int rdisdigit(int c)
{
    return (c >= '0' && c <= '9' ? 1 : 0);
}

#define UC(c)   ((unsigned char)c)

char rdisupper (unsigned char c)
{
    if ( c >= UC('A') && c <= UC('Z') )
        return 1;
    return 0;
}

char rdtolower(char c)
{
    if (rdisupper(c))
    {
        return c + ('a' - 'A');
    }
    return c;
}

int rdisalpha(int c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ? 1 : 0);
}

void rdstrrev(unsigned char *str)
{
    int i;
    int j;
    unsigned char a;
    unsigned len = strlen((const char *)str);
    for (i = 0, j = len - 1; i < j; i++, j--)
    {
        a = str[i];
        str[i] = str[j];
        str[j] = a;
    }
}

int rditoa(int num, unsigned char* str, int len, int base)
{
    int sum = num;
    int i = 0;
    int digit;
    if (len == 0)
        return -1;
    do
    {
        digit = sum % base;
        if (digit < 0xA)
            str[i++] = '0' + digit;
        else
            str[i++] = 'A' + digit - 0xA;
        sum /= base;
    }while (sum && (i < (len - 1)));
    if (i == (len - 1) && sum)
        return -1;
    str[i] = '\0';
    rdstrrev(str);
    return 0;
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
// long strtol(const char *nptr, char **endptr, register int base)
// {
//     register const char *s = nptr;
//     register unsigned long acc;
//     register int c;
//     register unsigned long cutoff;
//     register int neg = 0, any, cutlim;

//     /*
//      * Skip white space and pick up leading +/- sign if any.
//      * If base is 0, allow 0x for hex and 0 for octal, else
//      * assume decimal; if base is already 16, allow 0x.
//      */
//     do {
//         c = *s++;
//     } while (rdisspace(c));
//     if (c == '-') {
//         neg = 1;
//         c = *s++;
//     } else if (c == '+')
//         c = *s++;
//     if ((base == 0 || base == 16) &&
//         c == '0' && (*s == 'x' || *s == 'X')) {
//         c = s[1];
//         s += 2;
//         base = 16;
//     }
//     if (base == 0)
//         base = c == '0' ? 8 : 10;

//     /*
//      * Compute the cutoff value between legal numbers and illegal
//      * numbers.  That is the largest legal value, divided by the
//      * base.  An input number that is greater than this value, if
//      * followed by a legal input character, is too big.  One that
//      * is equal to this value may be valid or not; the limit
//      * between valid and invalid numbers is then based on the last
//      * digit.  For instance, if the range for longs is
//      * [-2147483648..2147483647] and the input base is 10,
//      * cutoff will be set to 214748364 and cutlim to either
//      * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
//      * a value > 214748364, or equal but the next digit is > 7 (or 8),
//      * the number is too big, and we will return a range error.
//      *
//      * Set any if any `digits' consumed; make it negative to indicate
//      * overflow.
//      */
//     cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
//     cutlim = cutoff % (unsigned long)base;
//     cutoff /= (unsigned long)base;
//     for (acc = 0, any = 0;; c = *s++) {
//         if (rdisdigit(c))
//             c -= '0';
//         else if (rdisalpha(c))
//             c -= rdisupper(c) ? 'A' - 10 : 'a' - 10;
//         else
//             break;
//         if (c >= base)
//             break;
//         if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
//             any = -1;
//         else {
//             any = 1;
//             acc *= base;
//             acc += c;
//         }
//     }
//     if (any < 0) {
//         acc = neg ? LONG_MIN : LONG_MAX;
//         // errno = ERANGE;
//     } else if (neg)
//         acc = -acc;
//     if (endptr != 0)
//         *endptr = (char *) (any ? s - 1 : nptr);
//     return (acc);
// }

// void* memcpy (void* dest, const void* src, size_t len)
// {
//     char *d = dest;
//     const char *s = src;
//     while (len--)
//        *d++ = *s++;
//     return dest;
// }

// void *memset (void* pBuffer, int nValue, size_t nLength)
// {
//     char *p = (char*)pBuffer;
//     while (nLength--)
//         *p++ = (char) nValue;
//     return pBuffer;
// }

void __cxa_pure_virtual()
{
}

// // Stubbed out exception routine to avoid unwind errors in ARM code
// // https://stackoverflow.com/questions/14028076/memory-utilization-for-unwind-support-on-arm-architecture
// void __aeabi_unwind_cpp_pr0(void) 
// {

// }

// // Stubbed out - probably should restore registers but might only be necessary on unwinding after exceptions
// // which are not used
// void __cxa_end_cleanup(void)
// {

// }

