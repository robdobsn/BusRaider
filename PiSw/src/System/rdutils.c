// Bus Raider
// Rob Dobson 2018

#include "../System/nmalloc.h"
#include "jsmnR.h"
#include "../System/ee_printf.h"
#include <string.h>

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

