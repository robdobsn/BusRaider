// Bus Raider
// Rob Dobson 2018

#include "rdutils.h"
#include "globaldefs.h"
#include "nmalloc.h"
#include "jsmnR.h"
#include "ee_printf.h"

int timer_isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime) {
        return (curTime > lastTime + maxDuration);
    }
    return (ULONG_MAX - (lastTime - curTime) > maxDuration);
}

// Heap space
extern unsigned int pheap_space;
extern unsigned int heap_sz;

void system_init()
{
    // Heap init
    nmalloc_set_memory_area((unsigned char*)(pheap_space), heap_sz);

}

char* strncpy(char* dest, const char* src, size_t num)
{
    while(*src && num--)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
    return dest;
}

int strncmp(const char* str1, const char* str2, size_t num)
{
    if (num == 0)
        return (0);
    do {
        if (*str1 != *str2++)
            return (*str1 - *(--str2));
        if (*str1++ == 0)
            break;
    } while (--num != 0);
    return (0);
}

bool jsonGetValueForKey(const char* srchKey, const char* jsonStr, char* pOutStr, int outStrMaxLen)
{
    // pCmdJson is a JSON string containing the command to execute (amongst other things possibly)
    #define MAX_TOKENS 20
    JSMNR_parser parser;
    jsmnrtok_t tokens[MAX_TOKENS];
    JSMNR_init(&parser);
    int tokenCountRslt = JSMNR_parse(&parser, jsonStr, strlen(jsonStr), tokens, MAX_TOKENS);
    if (tokenCountRslt < 0) {
        LogWrite("rdutils", LOG_DEBUG, "parseJson result %d maxTokens %d jsonLen %d\n", 
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
            strncpy(pOutStr, jsonStr + (pTok+1)->start, stringLen);
            // ee_printf("rdutils_json: srchkey %s value len %d val %s ll %d\n", srchKey, stringLen, pOutStr, strlen(pOutStr));
            return true;
        }
    }
    return false;
}