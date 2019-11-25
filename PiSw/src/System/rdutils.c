// Bus Raider
// Rob Dobson 2018

#include "../System/nmalloc.h"
#include "jsmnR.h"
#include "../System/logging.h"
#include <string.h>

bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen)
{
    #define MAX_TOKENS 500
    JSMNR_parser parser;
    jsmnrtok_t tokens[MAX_TOKENS];
    JSMNR_init(&parser);
    int tokenCountRslt = JSMNR_parse(&parser, jsonStr, strlen(jsonStr), tokens, MAX_TOKENS);
    if (tokenCountRslt < 0) {
        // LogWrite("rdutils", LOG_DEBUG, "jsonGetValueForKey keyStr %s jsonStr %s result %d maxTokens %d jsonLen %d", 
        //                 srchKey, jsonStr, tokenCountRslt, MAX_TOKENS, strlen(jsonStr));
        LogWrite("rdutils", LOG_DEBUG, "jsonGetValueForKey keyStr %s result %d maxTokens %d jsonLen %d", 
                        srchKey, tokenCountRslt, MAX_TOKENS, strlen(jsonStr));
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

int jsonGetArrayLen(const char* jsonStr)
{
    // Parse
    #define MAX_TOKENS 500
    JSMNR_parser parser;
    jsmnrtok_t tokens[MAX_TOKENS];
    JSMNR_init(&parser);
    int tokenCountRslt = JSMNR_parse(&parser, jsonStr, strlen(jsonStr), tokens, MAX_TOKENS);
    if (tokenCountRslt < 0) {
        LogWrite("rdutils", LOG_DEBUG, "jsonGetArrayLen result %d maxTokens %d jsonLen %d", 
                        tokenCountRslt, MAX_TOKENS, strlen(jsonStr));
        return false;
    }

    // Check if array
    if ((tokenCountRslt > 0) && (tokens[0].type == JSMNR_ARRAY))
    {
        // Return size (which is array length)
        return tokens[0].size;
    }

    // Not an array
    return -1;
}

bool jsonGetArrayElem(uint32_t arrayIdx, const char* jsonStr, char* pOutStr, int outStrMaxLen)
{
    strlcpy(pOutStr, "", outStrMaxLen);

    // Parse
    #define MAX_TOKENS 500
    JSMNR_parser parser;
    jsmnrtok_t tokens[MAX_TOKENS];
    JSMNR_init(&parser);
    int tokenCountRslt = JSMNR_parse(&parser, jsonStr, strlen(jsonStr), tokens, MAX_TOKENS);
    if (tokenCountRslt < 0) {
        LogWrite("rdutils", LOG_DEBUG, "jsonGetArrayElem result %d maxTokens %d jsonLen %d", 
                        tokenCountRslt, MAX_TOKENS, strlen(jsonStr));
        return false;
    }

    // Check if array and size big enough
    if ((tokenCountRslt > 0) && (tokens[0].type == JSMNR_ARRAY) && (tokens[0].size > (int)arrayIdx))
    {
        int tokenIdx = 1;
        // Iterate over array elems until we get to the one we want
        for (uint32_t i = 0; i < arrayIdx; i++)
        {
            int nextPos = tokens[tokenIdx].end;
            while (tokenIdx < tokenCountRslt)
            {
                if (tokens[tokenIdx].start > nextPos)
                    break;
                tokenIdx++;
            }
            if (tokenIdx >= tokenCountRslt)
                break;
        }
        // Check token valid
        if (tokenIdx >= tokenCountRslt)
            return false;
        // LogWrite("JSON", LOG_DEBUG, "Tok Type %d Start %d End %d Size %d", tokens[tokenIdx].type, tokens[tokenIdx].start, tokens[tokenIdx].end, tokens[tokenIdx].size);
        // Copy string out
        int maxLen = outStrMaxLen;
        if (maxLen > tokens[tokenIdx].end-tokens[tokenIdx].start+1)
            maxLen = tokens[tokenIdx].end-tokens[tokenIdx].start+1;
        strlcpy(pOutStr, jsonStr+tokens[tokenIdx].start, maxLen);
        return true;
    }

    // Not an array
    return false;
}

void jsonEscape(const char* inStr, char* outStr, int maxLen)
{
    const char* pIn = inStr;
    char* pOut = outStr;
    int curLen = 0;
    while (*pIn && (curLen + 2 < maxLen))
    {
        switch (*pIn) 
        {
            case '\x0a': *pOut++ = '\\'; *pOut++ = 'n'; curLen+=2; break;
            case '\x22': *pOut++ = '\\'; *pOut++ = '\"'; curLen+=2; break;
            case '\x5c': *pOut++ = '\\'; *pOut++ = '\\'; curLen+=2; break;
            default: *pOut++ = *pIn; curLen+=1; break;
        }
        pIn++;
    }
    if (curLen < maxLen)
        *pOut = 0;
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

char rdislower (unsigned char c)
{
    if ( c >= UC('a') && c <= UC('z') )
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

char rdtoupper(char c)
{
    if (rdislower(c))
    {
        return c - ('a' - 'A');
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

