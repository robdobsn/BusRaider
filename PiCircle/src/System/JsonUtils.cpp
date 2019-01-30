// JsonUtils
// Rob Dobson 2019

#include "JsonUtils.h"
#include "jsmnR.h"
#include "LogHandler.h"
#include "../System/StringUtils.h"

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
