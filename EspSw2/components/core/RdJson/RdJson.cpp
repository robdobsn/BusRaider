/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// JSON field extraction
// Many of the methods here support a dataPath parameter. This uses a syntax like a much simplified XPath:
// [0] returns the 0th element of an array
// / is a separator of nodes
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RdJson.h>
#include "Utils.h"

static const char *MODULE_PREFIX = "RdJson";

#define WARN_ON_PARSE_FAILURE
// #define DEBUG_PARSE_FAILURE
// #define DEBUG_GET_VALUES
// #define DEBUG_GET_ELEMENT
// #define DEBUG_GET_KEYS
// #define DEBUG_GET_ARRAY_ELEMS
// #define DEBUG_EXTRACT_NAME_VALUES
// #define DEBUG_FIND_KEY_IN_JSON
// #define DEBUG_IS_BOOLEAN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getElement
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getElement : Get location of element in JSON string
 * 
 * @param  {char*} dataPath       : path to element to return info about
 * @param  {int&} startPos        : [out] start position 
 * @param  {int&} strLen          : [out] length
 * @param  {jsmntype_t&} elemType : [out] element type
 * @param  {int&} elemSize        : [out] element size
 * @param  {char*} pSourceStr     : json string to search for element
 * @return {bool}                 : true if element found
 */
bool RdJson::getElement(const char *dataPath,
                        int &startPos, int &strLen,
                        jsmntype_t &elemType, int &elemSize,
                        const char *pSourceStr)
{
    // Check for null
    if (!pSourceStr)
    {
#ifdef DEBUG_GET_ELEMENT
        LOG_I(MODULE_PREFIX, "getElement failed null source");
#endif
        return false;
    }

    // Parse json into tokens
    int numTokens = 0;
    jsmntok_t *pTokens = parseJson(pSourceStr, numTokens);
    if (pTokens == NULL)
    {
#ifdef DEBUG_GET_ELEMENT
        LOG_I(MODULE_PREFIX, "getElement failedParse");
#endif
        return false;
    }

    // Find token
    int endTokenIdx = 0;
    int startTokenIdx = findKeyInJson(pSourceStr, pTokens, numTokens, 
                        dataPath, endTokenIdx);
    if (startTokenIdx < 0)
    {
#ifdef DEBUG_GET_ELEMENT
        LOG_I(MODULE_PREFIX, "getElement failed findKeyInJson");
#endif
        delete[] pTokens;
        return false;
    }

    // Extract information on element
    elemType = pTokens[startTokenIdx].type;
    elemSize = pTokens[startTokenIdx].size;
    startPos = pTokens[startTokenIdx].start;
    strLen = pTokens[startTokenIdx].end - startPos;
    delete[] pTokens;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getString
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getString : Get a string from the JSON
 * 
 * @param  {char*} dataPath       : path of element to return
 * @param  {char*} defaultValue   : default value to return
 * @param  {bool&} isValid        : [out] true if element found
 * @param  {jsmntype_t&} elemType : [out] type of element found (maybe a primitive or maybe object/array)
 * @param  {int&} elemSize        : [out] size of element found
 * @param  {char*} pSourceStr     : json to search
 * @return {String}               : found string value or default
 */
String RdJson::getString(const char *dataPath,
                         const char *defaultValue, bool &isValid,
                         jsmntype_t &elemType, int &elemSize,
                         const char *pSourceStr)
{
    // Find the element in the JSON
    int startPos = 0, strLen = 0;
    isValid = getElement(dataPath, startPos, strLen, elemType, elemSize, pSourceStr);
    if (!isValid)
        return defaultValue;

    // Extract string
    String outStr;
    char *pStr = safeStringDup(pSourceStr + startPos, strLen,
                               !(elemType == JSMN_STRING || elemType == JSMN_PRIMITIVE));
    if (pStr)
    {
        outStr = pStr;
        delete[] pStr;
    }

    // If the underlying element is a string or primitive value return size as length of string
    if (elemType == JSMN_STRING || elemType == JSMN_PRIMITIVE)
        elemSize = outStr.length();
    return outStr;
}

/**
 * getString 
 * 
 * @param  {char*} dataPath     : path of element to return
 * @param  {char*} defaultValue : default value
 * @param  {char*} pSourceStr   : json string to search
 * @param  {bool&} isValid      : [out] true if valid
 * @return {String}             : returned value or default
 */
String RdJson::getString(const char *dataPath, const char *defaultValue,
                         const char *pSourceStr, bool &isValid)
{
    jsmntype_t elemType = JSMN_UNDEFINED;
    int elemSize = 0;
    return getString(dataPath, defaultValue, isValid, elemType, elemSize,
                     pSourceStr);
}

// Alternate form of getString with fewer parameters
/**
 * getString 
 * 
 * @param  {char*} dataPath     : path of element to return
 * @param  {char*} defaultValue : default value
 * @param  {char*} pSourceStr   : json string to search
 * @return {String}             : returned value or default
 */
String RdJson::getString(const char *dataPath, const char *defaultValue,
                         const char *pSourceStr)
{
    bool isValid = false;
    jsmntype_t elemType = JSMN_UNDEFINED;
    int elemSize = 0;
    return getString(dataPath, defaultValue, isValid, elemType, elemSize,
                     pSourceStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getDouble
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * getDouble 
 * 
 * @param  {char*} dataPath      : path of element to return
 * @param  {double} defaultValue : default value
 * @param  {bool&} isValid       : [out] true if valid
 * @param  {char*} pSourceStr    : json string to search
 * @return {double}              : returned value or default
 */
double RdJson::getDouble(const char *dataPath,
                         double defaultValue, bool &isValid,
                         const char *pSourceStr)
{
    // Find the element in the JSON
    int startPos = 0, strLen = 0;
    jsmntype_t elemType = JSMN_UNDEFINED;
    int elemSize = 0;
    isValid = getElement(dataPath, startPos, strLen, elemType, elemSize, pSourceStr);
    if (!isValid)
        return defaultValue;
    // Check for booleans
    int retValue = 0;
    if (RdJson::isBoolean(pSourceStr+startPos, strLen, retValue))
        return retValue;
    return strtod(pSourceStr + startPos, NULL);
}
/**
 * getDouble 
 * 
 * @param  {char*} dataPath      : path of element to return
 * @param  {double} defaultValue : default value
 * @param  {char*} pSourceStr    : json string to search
 * @return {double}              : returned value or default
 */
double RdJson::getDouble(const char *dataPath, double defaultValue,
                         const char *pSourceStr)
{
    bool isValid = false;
    return getDouble(dataPath, defaultValue, isValid, pSourceStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getLong
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getLong
 * 
 * @param  {char*} dataPath    : path of element to return
 * @param  {long} defaultValue : default value
 * @param  {bool&} isValid     : [out] true if valid
 * @param  {char*} pSourceStr  : json string to search
 * @return {long}              : returned value or default
 */
long RdJson::getLong(const char *dataPath,
                     long defaultValue, bool &isValid,
                     const char *pSourceStr)
{
    // Find the element in the JSON
    int startPos = 0, strLen = 0;
    jsmntype_t elemType = JSMN_UNDEFINED;
    int elemSize = 0;
    isValid = getElement(dataPath, startPos, strLen, elemType, elemSize, pSourceStr);
    if (!isValid)
        return defaultValue;
    // Check for booleans
    int retValue = 0;
    if (RdJson::isBoolean(pSourceStr+startPos, strLen, retValue))
        return retValue;
    return strtol(pSourceStr + startPos, NULL, 0);
}

/**
 * getLong 
 * 
 * @param  {char*} dataPath    : path of element to return
 * @param  {long} defaultValue : default value
 * @param  {char*} pSourceStr  : json string to search
 * @return {long}              : returned value or default
 */
long RdJson::getLong(const char *dataPath, long defaultValue, const char *pSourceStr)
{
    bool isValid = false;
    return getLong(dataPath, defaultValue, isValid, pSourceStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getLong
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char *RdJson::getElemTypeStr(jsmntype_t type)
{
    switch (type)
    {
    case JSMN_PRIMITIVE:
        return "PRIMITIVE";
    case JSMN_STRING:
        return "STRING";
    case JSMN_OBJECT:
        return "OBJECT";
    case JSMN_ARRAY:
        return "ARRAY";
    case JSMN_UNDEFINED:
        return "UNDEFINED";
    }
    return "UNKNOWN";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getType of outer element of JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getType of outer element of JSON 
 * 
 * @param  {int&} arrayLen    : length of array or object
 * @param  {char*} pSourceStr : json string to search
 * @return {jsmntype_t}       : returned value is the type of the object
 */
jsmntype_t RdJson::getType(int &arrayLen, const char *pSourceStr)
{
    arrayLen = 0;
    // Check for null
    if (!pSourceStr)
        return JSMN_UNDEFINED;

    // Parse json into tokens
    int numTokens = 0;
    jsmntok_t *pTokens = parseJson(pSourceStr, numTokens);
    if (pTokens == NULL)
        return JSMN_UNDEFINED;

    // Get the type of the first token
    arrayLen = pTokens->size;
    jsmntype_t typ = pTokens->type;
    delete pTokens;
    return typ;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getKeys
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getKeys 
 * 
 * @param  {char*} dataPath                 : path of element to return
 * @param  {std::vector<String>} keysVector : keys of object to return
 * @param  {char*} pSourceStr               : json string to search
 * @return {bool}                           : true if valid
 */
bool RdJson::getKeys(const char *dataPath, std::vector<String>& keysVector, const char *pSourceStr)
{
    // Check for null
    if (!pSourceStr)
        return false;

    // Parse json into tokens
    int numTokens = 0;
    jsmntok_t *pTokens = parseJson(pSourceStr, numTokens);
    if (pTokens == NULL)
    {
        return false;
    }

    // Debug
    // debugDumpParseResult(pSourceStr, pTokens, numTokens);

    // Find token
    int endTokenIdx = 0;
    int startTokenIdx = findKeyInJson(pSourceStr, pTokens, numTokens, 
                        dataPath, endTokenIdx);
    if (startTokenIdx < 0)
    {
        delete[] pTokens;
        return false;
    }

    //Debug
#ifdef DEBUG_GET_KEYS
    LOG_I(MODULE_PREFIX, "Found elem startTok %d endTok %d", startTokenIdx, endTokenIdx);
#endif

    // Check its an object
    if ((pTokens[startTokenIdx].type != JSMN_OBJECT) || (pTokens[startTokenIdx].size > MAX_KEYS_TO_RETURN))
    {
        delete[] pTokens;
        return false;
    }
    int numKeys = pTokens[startTokenIdx].size;

    // Extract the keys of the object
    keysVector.resize(numKeys);
    unsigned int tokIdx = startTokenIdx+1;
    String keyStr;
    for (int keyIdx = 0; keyIdx < numKeys; keyIdx++)
    {
        // Check valid
        if ((tokIdx >= numTokens) || (pTokens[tokIdx].type != JSMN_STRING))
            break;

        // Extract the string
        Utils::strFromBuffer((uint8_t*)pSourceStr+pTokens[tokIdx].start, pTokens[tokIdx].end-pTokens[tokIdx].start, keyStr);
        keysVector[keyIdx] = keyStr;
        
        // Find end of value
#ifdef DEBUG_GET_KEYS
        LOG_I(MODULE_PREFIX, "getKeys Looking for end of tokIdx %d", tokIdx);
#endif
        tokIdx = findElemEnd(pSourceStr, pTokens, numTokens, tokIdx+1);
#ifdef DEBUG_GET_KEYS
        LOG_I(MODULE_PREFIX, "getKeys ............. Found end at tokIdx %d", tokIdx);
#endif
    }

    // Clean up
    delete[] pTokens;
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getArrayElems
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * getArrayElems 
 * 
 * @param  {char*} dataPath                 : path of element to return
 * @param  {std::vector<String>} arrayElems : elems of array to return
 * @param  {char*} pSourceStr               : json string to search
 * @return {bool}                           : true if valid
 */
bool RdJson::getArrayElems(const char *dataPath, std::vector<String>& arrayElems, const char *pSourceStr)
{
    // Check for null
    if (!pSourceStr)
        return false;

    // Parse json into tokens
    int numTokens = 0;
    jsmntok_t *pTokens = parseJson(pSourceStr, numTokens);
    if (pTokens == NULL)
    {
        return false;
    }

    // Find token
    int endTokenIdx = 0;
    int startTokenIdx = findKeyInJson(pSourceStr, pTokens, numTokens, 
                        dataPath, endTokenIdx);
    if (startTokenIdx < 0)
    {
        delete[] pTokens;
        return false;
    }

    //Debug
#ifdef DEBUG_GET_ARRAY_ELEMS
    LOG_I(MODULE_PREFIX, "Found elem startTok %d endTok %d", startTokenIdx, endTokenIdx);
#endif

    // Check its an array
    if ((pTokens[startTokenIdx].type != JSMN_ARRAY) || (pTokens[startTokenIdx].size > MAX_KEYS_TO_RETURN))
    {
        delete[] pTokens;
        return false;
    }
    int numElems = pTokens[startTokenIdx].size;

    // Extract the array elements as strings (regardless of type)
    arrayElems.resize(numElems);
    unsigned int tokIdx = startTokenIdx+1;
    String elemStr;
    for (int elemIdx = 0; elemIdx < numElems; elemIdx++)
    {
        // Check valid
        if (tokIdx >= numTokens)
            break;

        // Extract the elem
        Utils::strFromBuffer((uint8_t*)pSourceStr+pTokens[tokIdx].start, pTokens[tokIdx].end-pTokens[tokIdx].start, elemStr);
        arrayElems[elemIdx] = elemStr;
        
        // Find end of elem
        tokIdx = findElemEnd(pSourceStr, pTokens, numTokens, tokIdx);
    }

    // Clean up
    delete[] pTokens;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parseJson
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * jsmntok_t* RdJson::parseJson 
 * 
 * @param  {char*} jsonStr  : json string to search
 * @param  {int&} numTokens : [out] number of tokens found
 * @param  {int} maxTokens  : max number of tokens to return
 * @return {jsmntok_t*}     : tokens found - NOTE - pointer must be delete[] by caller
 */
jsmntok_t *RdJson::parseJson(const char *jsonStr, int &numTokens,
                              int maxTokens)
{
    // Check for null source string
    if (jsonStr == NULL)
    {
        LOG_E(MODULE_PREFIX, "parseJson input is NULL");
        return NULL;
    }

    // Find how many tokens in the string
    jsmn_parser parser;
    jsmn_init(&parser);
    int tokenCountRslt = jsmn_parse(&parser, jsonStr, strlen(jsonStr),
                                     NULL, maxTokens);
    if (tokenCountRslt < 0)
    {
#ifdef DEBUG_PARSE_FAILURE
        LOG_I(MODULE_PREFIX, "parseJson result %d maxTokens %d jsonLen %d jsonStr %s", tokenCountRslt, 
                        maxTokens, strlen(jsonStr), jsonStr);
#endif
        jsmn_logLongStr("RdJson: jsonStr", jsonStr, false);
        return NULL;
    }

    // Allocate space for tokens
    if (tokenCountRslt > maxTokens)
        tokenCountRslt = maxTokens;
    jsmntok_t *pTokens = new jsmntok_t[tokenCountRslt];

    // Parse again
    jsmn_init(&parser);
    tokenCountRslt = jsmn_parse(&parser, jsonStr, strlen(jsonStr),
                                 pTokens, tokenCountRslt);
    if (tokenCountRslt < 0)
    {
#ifdef WARN_ON_PARSE_FAILURE
        LOG_W(MODULE_PREFIX, "parseJson result: %d", tokenCountRslt);
#endif
#ifdef DEBUG_PARSE_FAILURE
        LOG_I(MODULE_PREFIX, "parseJson jsonStr %s numTok %d maxTok %d", jsonStr, numTokens, maxTokens);
#endif
        delete[] pTokens;
        return NULL;
    }
    numTokens = tokenCountRslt;
    return pTokens;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// validateJson
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdJson::validateJson(const char* pSourceStr, int& numTokens)
{
    // Check for null source string
    if (pSourceStr == NULL)
    {
#ifdef DEBUG_PARSE_FAILURE
        LOG_I(MODULE_PREFIX, "validateJson input is NULL");
#endif
        return false;
    }

    // Find how many tokens in the string
    jsmn_parser parser;
    jsmn_init(&parser);
    numTokens = jsmn_parse(&parser, pSourceStr, strlen(pSourceStr),
                                     NULL, RDJSON_MAX_TOKENS);
    if (numTokens < 0)
    {
#ifdef DEBUG_PARSE_FAILURE
        LOG_I(MODULE_PREFIX, "validateJson result %d maxTokens %d jsonLen %d", 
                numTokens, RDJSON_MAX_TOKENS, strlen(pSourceStr));
#endif
        jsmn_logLongStr("RdJson: jsonStr", pSourceStr, false);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// findElemEnd
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * findElemEnd 
 * 
 * @param  {char*} jsonOriginal   : json to search
 * @param  {jsmntok_t []} tokens  : tokens from jsmnr parser
 * @param  {unsigned} int         : count of tokens from parser
 * @param  {int} startTokenIdx    : token index to start search from
 * @return {int}                  : token index of token after the element
 *                                : OR numTokens+1 if element occupies rest of json
 *                                : OR -1 on error
 */
int RdJson::findElemEnd(const char *jsonOriginal, jsmntok_t tokens[],
                          unsigned int numTokens, int startTokenIdx)
{
    // Check valid
    if ((startTokenIdx < 0) || (startTokenIdx >= numTokens))
        return -1;

    // Check for outer object - parentTokenIdx == -1
    int parentTokIdx = tokens[startTokenIdx].parent;
    if (parentTokIdx == -1)
        return numTokens;

    // Handle simple elements
    switch(tokens[startTokenIdx].type)
    {
        case JSMN_PRIMITIVE:
        case JSMN_STRING:
            return startTokenIdx+1;
        default:
            break;
    }

    // Handle arrays and objects - look for element with parent the same or lower
    for (int srchTokIdx = startTokenIdx+1; srchTokIdx < numTokens; srchTokIdx++)
    {
        if (tokens[srchTokIdx].parent <= parentTokIdx)
        {
            return srchTokIdx;
        }
    }
    return numTokens;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// findArrayElem
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * findArrayElem 
 * 
 * @param  {char*} jsonOriginal  : json to search
 * @param  {jsmntok_t []} tokens : tokens from jsmnr parser
 * @param  {unsigned} numTokens  : count of tokens from parser
 * @param  {int} startTokenIdx   : token index to start search from
 * @param  {int} arrayElemIdx    : 
 * @return {int}                 : 
 */
int RdJson::findArrayElem(const char *jsonOriginal, jsmntok_t tokens[],
                          unsigned int numTokens, int startTokenIdx, 
                          int arrayElemIdx)
{
    // Check valid
    if ((startTokenIdx < 0) || (startTokenIdx >= numTokens-1))
        return -1;

    // Check this is an array
    if (tokens[startTokenIdx].type != JSMN_ARRAY)
        return -1;

    // // All top-level array elements have the array token as their parent
    // int parentTokIdx = startTokenIdx;

    // Check index is valid
    if (tokens[startTokenIdx].size <= arrayElemIdx)
        return -1; 

    // Loop over elements
    int elemTokIdx = startTokenIdx + 1;
    for (int i = 0; i  < arrayElemIdx; i++)
    {
        elemTokIdx = findElemEnd(jsonOriginal, tokens, numTokens, elemTokIdx);
    }
    return elemTokIdx;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// findKeyInJson
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * findKeyInJson : find an element in a json string using a search path 
 * 
 * @param  {char*} jsonOriginal     : json string to search
 * @param  {jsmntok_t []} tokens    : tokens from jsmnr parser
 * @param  {unsigned int} numTokens : number of tokens
 * @param  {char*} dataPath         : path of searched for element
 * @param  {int} endTokenIdx        : token index to end search
 * @param  {jsmntype_t} keyType     : type of json element to find
 * @return {int}                    : index of found token or -1 if failed
 */
int RdJson::findKeyInJson(const char *jsonOriginal, jsmntok_t tokens[],
                          unsigned int numTokens, const char *dataPath,
                          int &endTokenIdx,
                          jsmntype_t keyType)
{
    // TODO - fixed size buffer - review to ensure ok
    const int MAX_SRCH_KEY_LEN = 150;
    char srchKey[MAX_SRCH_KEY_LEN + 1];
    const char *pDataPathPos = dataPath;
    int dataPathLen = strlen(dataPath);
    // Note that the root element has index 0
    int curTokenIdx = 0;
    int maxTokenIdx = numTokens - 1;
    bool atNodeLevel = false;

    // Go through the parts of the path to find the whole
    while (pDataPathPos <= dataPath + dataPathLen)
    {
        // Get the next part of the path
        const char *slashPos = strstr(pDataPathPos, "/");
        if (slashPos == NULL)
        {
            safeStringCopy(srchKey, pDataPathPos, MAX_SRCH_KEY_LEN);
            pDataPathPos += strlen(pDataPathPos) + 1;
            atNodeLevel = true;
        }
        else if (slashPos - pDataPathPos >= MAX_SRCH_KEY_LEN)
        {
            safeStringCopy(srchKey, pDataPathPos, MAX_SRCH_KEY_LEN);
            pDataPathPos = slashPos + 1;
        }
        else
        {
            safeStringCopy(srchKey, pDataPathPos, slashPos - pDataPathPos);
            pDataPathPos = slashPos + 1;
        }
#ifdef DEBUG_FIND_KEY_IN_JSON
        LOG_I(MODULE_PREFIX, "findKeyInJson slashPos %ld, %d, srchKey <%s>", (long)slashPos, slashPos-pDataPathPos, srchKey);
#endif

        // See if search key contains an array reference
        bool arrayElementReqd = false;
        int reqdArrayIdx = 0;
        char *sqBracketPos = strstr(srchKey, "[");
        if (sqBracketPos != NULL)
        {
            // Extract array index
            long arrayIdx = strtol(sqBracketPos + 1, NULL, 10);
            if (arrayIdx >= 0)
            {
                arrayElementReqd = true;
                reqdArrayIdx = (int)arrayIdx;
            }
            // Truncate the string at the square bracket
            *sqBracketPos = 0;
        }

#ifdef DEBUG_FIND_KEY_IN_JSON
        LOG_I(MODULE_PREFIX, "findKeyInJson srchKey %s arrayIdx %d", srchKey, reqdArrayIdx);
#endif
        // Iterate over tokens to find key of the right type
        // If we are already looking at the node level then search for requested type
        // Otherwise search for an element that will contain the next level key
        jsmntype_t keyTypeToFind = atNodeLevel ? keyType : JSMN_STRING;
        for (int tokIdx = curTokenIdx; tokIdx <= maxTokenIdx;)
        {
            // See if the key matches - this can either be a string match on an object key or
            // just an array element match (with an empty key)
            jsmntok_t *pTok = tokens + tokIdx;
            bool keyMatchFound = false;
            if ((pTok->type == JSMN_STRING) && ((int)strlen(srchKey) == pTok->end - pTok->start) && (strncmp(jsonOriginal + pTok->start, srchKey, pTok->end - pTok->start) == 0))
            {
                keyMatchFound = true;
                tokIdx += 1;
                pTok = tokens + tokIdx;
            }
            else if (((pTok->type == JSMN_ARRAY) || (pTok->type == JSMN_OBJECT)) && ((int)strlen(srchKey) == 0))
            {
                keyMatchFound = true;
            }
#ifdef DEBUG_FIND_KEY_IN_JSON
            LOG_I(MODULE_PREFIX, "findKeyInJson tokIdx %d Token type %d srchKey %s arrayReqd %d reqdIdx %d matchFound %d", 
                            tokIdx, pTok->type, srchKey, arrayElementReqd, reqdArrayIdx, keyMatchFound);
#endif

            if (keyMatchFound)
            {
                // We have found the matching key so now for the contents ...

                // Check if we were looking for an array element
                if (arrayElementReqd)
                {
                    if (tokens[tokIdx].type == JSMN_ARRAY)
                    {
                        int newTokIdx = findArrayElem(jsonOriginal, tokens, numTokens, tokIdx, reqdArrayIdx);
#ifdef DEBUG_FIND_KEY_IN_JSON
                        LOG_I(MODULE_PREFIX, "findKeyInJson TokIdxArray inIdx %d, reqdArrayIdx %d, outTokIdx %d", 
                                        tokIdx, reqdArrayIdx, newTokIdx);
#endif
                        tokIdx = newTokIdx;
                    }
                    else
                    {
                        // This isn't an array element
#ifdef DEBUG_FIND_KEY_IN_JSON
                        LOG_I(MODULE_PREFIX, "findKeyInJson NOT AN ARRAY ELEM");
#endif
                        return -1;
                    }
                }

                // atNodeLevel indicates that we are now at the level of the JSON tree that the user requested
                // - so we should be extracting the value referenced now
                if (atNodeLevel)
                {
                    // LOG_I(MODULE_PREFIX, "findKeyInJson we have got it %d", tokIdx);
                    if ((keyTypeToFind == JSMN_UNDEFINED) || (tokens[tokIdx].type == keyTypeToFind))
                    {
                        endTokenIdx = findElemEnd(jsonOriginal, tokens, numTokens, tokIdx);
                        return tokIdx;
                    }
#ifdef DEBUG_FIND_KEY_IN_JSON
                    LOG_I(MODULE_PREFIX, "findKeyInJson AT NOTE LEVEL FAIL");
#endif
                    return -1;
                }
                else
                {
                    // Check for an object
#ifdef DEBUG_FIND_KEY_IN_JSON
                    LOG_I(MODULE_PREFIX, "findKeyInJson findElemEnd inside tokIdx %d", tokIdx);
#endif
                    if ((tokens[tokIdx].type == JSMN_OBJECT) || (tokens[tokIdx].type == JSMN_ARRAY))
                    {
                        // Continue next level of search in this object
                        maxTokenIdx = findElemEnd(jsonOriginal, tokens, numTokens, tokIdx);
                        curTokenIdx = (tokens[tokIdx].type == JSMN_OBJECT) ? tokIdx + 1 : tokIdx;
#ifdef DEBUG_FIND_KEY_IN_JSON
                        LOG_I(MODULE_PREFIX, "findKeyInJson tokIdx %d max %d next %d", 
                                    tokIdx, maxTokenIdx, curTokenIdx);
#endif
                        break;
                    }
                    else
                    {
                        // Found a key in the path but it didn't point to an object so we can't continue
#ifdef DEBUG_FIND_KEY_IN_JSON
                        LOG_I(MODULE_PREFIX, "findKeyInJson FOUND KEY BUT NOT POINTING TO OBJ");
#endif
                        return -1;
                    }
                }
            }
            else if (pTok->type == JSMN_STRING)
            {
                // We're at a key string but it isn't the one we want so skip its contents
                tokIdx = findElemEnd(jsonOriginal, tokens, numTokens, tokIdx+1);
            }
            else if (pTok->type == JSMN_OBJECT)
            {
                // Move to the first key of the object
                tokIdx++;
            }
            else if (pTok->type == JSMN_ARRAY)
            {
                // Root level array which doesn't match the dataPath
#ifdef DEBUG_FIND_KEY_IN_JSON
                LOG_I(MODULE_PREFIX, "findKeyInJson UNEXPECTED ARRAY");
#endif
                return -1;
            }
            else
            {
                // Shouldn't really get here as all keys are strings
                tokIdx++;
            }
        }
    }
#ifdef DEBUG_FIND_KEY_IN_JSON
    LOG_I(MODULE_PREFIX, "findKeyInJson DROPPED OUT");
#endif
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * escapeString 
 * 
 * @param  {String} strToEsc : string in which to replace characters which are invalid in JSON
 */
void RdJson::escapeString(String &strToEsc)
{
    // Replace characters which are invalid in JSON
    strToEsc.replace("\\", "\\\\");
    strToEsc.replace("\"", "\\\"");
    strToEsc.replace("\n", "\\n");
}
/**
 * unescapeString 
 * 
 * @param  {String} strToUnEsc : string in which to restore characters which are invalid in JSON
 */
void RdJson::unescapeString(String &strToUnEsc)
{
    // Replace characters which are invalid in JSON
    strToUnEsc.replace("\\\"", "\"");
    strToUnEsc.replace("\\\\", "\\");
    strToUnEsc.replace("\\n", "\n");
}

size_t RdJson::safeStringLen(const char *pSrc,
                             bool skipJSONWhitespace, size_t maxx)
{
    if (maxx == 0)
        return 0;
    const char *pS = pSrc;
    int stringLen = 0;
    bool insideDoubleQuotes = false;
    bool insideSingleQuotes = false;
    size_t charsTakenFromSrc = 0;
    while (*pS)
    {
        char ch = *pS++;
        charsTakenFromSrc++;
        if ((ch == '\'') && !insideDoubleQuotes)
            insideSingleQuotes = !insideSingleQuotes;
        else if ((ch == '\"') && !insideSingleQuotes)
            insideDoubleQuotes = !insideDoubleQuotes;
        else if ((!insideDoubleQuotes) && (!insideSingleQuotes) && skipJSONWhitespace && isspace(ch))
            continue;
        stringLen++;
        if (maxx != LONG_MAX && charsTakenFromSrc >= maxx)
            return stringLen;
    }
    return stringLen;
}

void RdJson::safeStringCopy(char *pDest, const char *pSrc,
                            size_t maxx, bool skipJSONWhitespace)
{
    char *pD = pDest;
    const char *pS = pSrc;
    size_t srcStrlen = strlen(pS);
    size_t stringLen = 0;
    bool insideDoubleQuotes = false;
    bool insideSingleQuotes = false;
    for (size_t i = 0; i < srcStrlen + 1; i++)
    {
        char ch = *pS++;
        if ((ch == '\'') && !insideDoubleQuotes)
            insideSingleQuotes = !insideSingleQuotes;
        else if ((ch == '\"') && !insideSingleQuotes)
            insideDoubleQuotes = !insideDoubleQuotes;
        else if ((!insideDoubleQuotes) && (!insideSingleQuotes) && skipJSONWhitespace && (isspace(ch) || (ch > 0 && ch < 32) || (ch >= 127)))
        {
            continue;
        }
        *pD++ = ch;
        stringLen++;
        if (stringLen >= maxx)
        {
            *pD = 0;
            break;
        }
    }
}

char *RdJson::safeStringDup(const char *pSrc, size_t maxx,
                            bool skipJSONWhitespace)
{
    size_t toAlloc = safeStringLen(pSrc, skipJSONWhitespace, maxx);
    char *pDest = new char[toAlloc + 1];
    if (!pDest)
        return pDest;
    pDest[toAlloc] = 0;
    if (toAlloc == 0)
        return pDest;
    char *pD = pDest;
    const char *pS = pSrc;
    size_t srcStrlen = strlen(pS);
    size_t stringLen = 0;
    bool insideDoubleQuotes = false;
    bool insideSingleQuotes = false;
    for (size_t i = 0; i < srcStrlen + 1; i++)
    {
        char ch = *pS++;
        if ((ch == '\'') && !insideDoubleQuotes)
            insideSingleQuotes = !insideSingleQuotes;
        else if ((ch == '\"') && !insideSingleQuotes)
            insideDoubleQuotes = !insideDoubleQuotes;
        else if ((!insideDoubleQuotes) && (!insideSingleQuotes) && skipJSONWhitespace && isspace(ch))
            continue;
        *pD++ = ch;
        stringLen++;
        if (stringLen >= maxx || stringLen >= toAlloc)
        {
            *pD = 0;
            break;
        }
    }
    // LOG_D(MODULE_PREFIX, "safeStringDup <%s> %d %d %d %d %d %d %d <%s>", pSrc, maxx, toAlloc, srcStrlen, stringLen, insideDoubleQuotes, insideSingleQuotes, skipJSONWhitespace, pDest);
    return pDest;
}

void RdJson::debugDumpParseResult(const char* pSourceStr, jsmntok_t* pTokens, int numTokens)
{
    for (int i = 0; i < numTokens; i++)
    {
        String elemStr;
        Utils::strFromBuffer((uint8_t*)pSourceStr+pTokens[i].start, pTokens[i].end-pTokens[i].start, elemStr);
        LOG_I(MODULE_PREFIX, "Token %2d type %9s size %d start %4d end %4d parent %3d str %s",
                i, getElemTypeStr(pTokens[i].type), pTokens[i].size, pTokens[i].start, pTokens[i].end,
                pTokens[i].parent, elemStr.c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert name value pairs to JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdJson::getJSONFromNVPairs(std::vector<NameValuePair>& nameValuePairs, bool includeOuterBraces)
{
    // Calculate length for efficiency
    uint32_t reserveLen = 0;
    for (NameValuePair& pair : nameValuePairs)
        reserveLen += 6 + pair.name.length() + pair.value.length();

    // Generate JSON
    String jsonStr;
    jsonStr.reserve(reserveLen);
    for (NameValuePair& pair : nameValuePairs)
    {
        if (jsonStr.length() > 0)
            jsonStr += ',';
        if (pair.value.startsWith("[") || pair.value.startsWith("{"))
            jsonStr += "\"" + pair.name + "\":" + pair.value;
        else
            jsonStr += "\"" + pair.name + "\":\"" + pair.value + "\"";
    }
    if (includeOuterBraces)
        return "{" + jsonStr + "}";
    return jsonStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert JSON object to HTML query string syntax
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdJson::getHTMLQueryFromJSON(const String& jsonStr)
{
    // Get keys of object
    std::vector<String> keyStrs;
    RdJson::getKeys("", keyStrs, jsonStr.c_str());
    if (keyStrs.size() == 0)
        return "";

    // Fill object
    String outStr;
    for (String& keyStr : keyStrs)
    {
        String valStr = getString(keyStr.c_str(), "", jsonStr.c_str());
        if (valStr.length() == 0)
            continue;
        if (outStr.length() != 0)
            outStr += "&";
        outStr += keyStr + "=" + valStr;
    }
    return outStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extract name-value pairs from string
// nameValueSep - e.g. "=" for HTTP
// pairDelim - e.g. "&" for HTTP
// pairDelimAlt - e.g. ";" for HTTP alternate (pass 0 if not needed)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdJson::extractNameValues(const String& inStr, 
        const char* pNameValueSep, const char* pPairDelim, const char* pPairDelimAlt, 
        std::vector<RdJson::NameValuePair>& nameValuePairs)
{
   // Count the pairs
    uint32_t pairCount = 0;
    const char* pCurSep = inStr.c_str();
    while(pCurSep)
    {
        pCurSep = strstr(pCurSep, pNameValueSep);
        if (pCurSep)
        {
            pairCount++;
            pCurSep++;
        }
    }

#ifdef DEBUG_EXTRACT_NAME_VALUES
    // Debug
    LOG_I(MODULE_PREFIX, "extractNameValues found %d nameValues", pairCount);
#endif

    // Extract the pairs
    nameValuePairs.resize(pairCount);
    pCurSep = inStr.c_str();
    bool sepTypeIsEqualsSign = true;
    uint32_t pairIdx = 0;
    String name, val;
    while(pCurSep)
    {
        // Each pair has the form "name=val;" (semicolon missing on last pair)
        const char* pElemStart = pCurSep;
        if (sepTypeIsEqualsSign)
        {
            // Check for missing =
            pCurSep = strstr(pElemStart, pNameValueSep);
            if (!pCurSep)
                break;
            Utils::strFromBuffer((uint8_t*)pElemStart, pCurSep-pElemStart, name);
            pCurSep++;
        }
        else
        {
            // Handle two alternatives - sep or no sep
            pCurSep = strstr(pElemStart, pPairDelim);
            if (!pCurSep && pPairDelimAlt)
                pCurSep = strstr(pElemStart, pPairDelimAlt);
            if (pCurSep)
            {
                Utils::strFromBuffer((uint8_t*)pElemStart, pCurSep-pElemStart, val);
                pCurSep++;
            }
            else
            {
                val = pElemStart;
            }
        }

        // Next separator
        sepTypeIsEqualsSign = !sepTypeIsEqualsSign;
        if (!sepTypeIsEqualsSign)
            continue;

        // Store and move on
        if (pairIdx >= pairCount)
            break;
        name.trim();
        val.trim();
        nameValuePairs[pairIdx] = {name,val};
        pairIdx++;
    }

#ifdef DEBUG_EXTRACT_NAME_VALUES
    // Debug
    for (RdJson::NameValuePair& pair : nameValuePairs)
        LOG_I(MODULE_PREFIX, "extractNameValues name %s val %s", pair.name.c_str(), pair.value.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdJson::isBoolean(const char* pBuf, uint32_t bufLen, int &retValue)
{
    if ((*pBuf == 'f') || (*pBuf == 't'))
    {
        String elemStr;
        Utils::strFromBuffer((uint8_t*)pBuf, bufLen, elemStr);
#ifdef DEBUG_IS_BOOLEAN
        LOG_I(MODULE_PREFIX, "isBoolean str %s", elemStr.c_str());
#endif
        if (elemStr.equals("true"))
        {
            retValue = 1;
            return true;
        }
        else if (elemStr.equals("false"))
        {
            retValue = 0;
            return true;
        }
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Re-create JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef RDJSON_RECREATE_JSON
int RdJson::recreateJson(const char *js, jsmntok_t *t,
                         size_t count, int indent, String &outStr)
{
    int i, j, k;

    if (count == 0)
    {
        return 0;
    }
    if (t->type == JSMN_PRIMITIVE)
    {
        LOG_D(MODULE_PREFIX, "recreateJson Found primitive size %d, start %d, end %d",
                  t->size, t->start, t->end);
        LOG_D(MODULE_PREFIX, "recreateJson %.*s", t->end - t->start, js + t->start);
        char *pStr = safeStringDup(js + t->start,
                                   t->end - t->start);
        outStr.concat(pStr);
        delete[] pStr;
        return 1;
    }
    else if (t->type == JSMN_STRING)
    {
        LOG_D(MODULE_PREFIX, "recreateJson Found string size %d, start %d, end %d",
                  t->size, t->start, t->end);
        LOG_D(MODULE_PREFIX, "recreateJson '%.*s'", t->end - t->start, js + t->start);
        char *pStr = safeStringDup(js + t->start,
                                   t->end - t->start);
        outStr.concat("\"");
        outStr.concat(pStr);
        outStr.concat("\"");
        delete[] pStr;
        return 1;
    }
    else if (t->type == JSMN_OBJECT)
    {
        LOG_D(MODULE_PREFIX, "recreateJson Found object size %d, start %d, end %d",
                  t->size, t->start, t->end);
        j = 0;
        outStr.concat("{");
        for (i = 0; i < t->size; i++)
        {
            for (k = 0; k < indent; k++)
            {
                LOG_D(MODULE_PREFIX, "  ");
            }
            j += recreateJson(js, t + 1 + j, count - j, indent + 1, outStr);
            outStr.concat(":");
            LOG_D(MODULE_PREFIX, ": ");
            j += recreateJson(js, t + 1 + j, count - j, indent + 1, outStr);
            LOG_D(MODULE_PREFIX, "");
            if (i != t->size - 1)
            {
                outStr.concat(",");
            }
        }
        outStr.concat("}");
        return j + 1;
    }
    else if (t->type == JSMN_ARRAY)
    {
        LOG_D(MODULE_PREFIX, "#Found array size %d, start %d, end %d",
                  t->size, t->start, t->end);
        j = 0;
        outStr.concat("[");
        LOG_D(MODULE_PREFIX, "");
        for (i = 0; i < t->size; i++)
        {
            for (k = 0; k < indent - 1; k++)
            {
                LOG_D(MODULE_PREFIX, "  ");
            }
            LOG_D(MODULE_PREFIX, "   - ");
            j += recreateJson(js, t + 1 + j, count - j, indent + 1, outStr);
            if (i != t->size - 1)
            {
                outStr.concat(",");
            }
            LOG_D(MODULE_PREFIX, "");
        }
        outStr.concat("]");
        return j + 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Print JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdJson::doPrint(const char *jsonStr)
{
    jsmn_parser parser;
    jsmn_init(&parser);
    int tokenCountRslt = jsmn_parse(&parser, jsonStr, strlen(jsonStr),
                                     NULL, 1000);
    if (tokenCountRslt < 0)
    {
        LOG_I(MODULE_PREFIX, "JSON parse result: %d", tokenCountRslt);
        return false;
    }
    jsmntok_t *pTokens = new jsmntok_t[tokenCountRslt];
    jsmn_init(&parser);
    tokenCountRslt = jsmn_parse(&parser, jsonStr, strlen(jsonStr),
                                 pTokens, tokenCountRslt);
    if (tokenCountRslt < 0)
    {
        LOG_I(MODULE_PREFIX, "JSON parse result: %d", tokenCountRslt);
        delete pTokens;
        return false;
    }
    // Top level item must be an object
    if (tokenCountRslt < 1 || pTokens[0].type != JSMN_OBJECT)
    {
        LOG_E(MODULE_PREFIX "JSON must have top level object");
        delete pTokens;
        return false;
    }
    LOG_D(MODULE_PREFIX "Dumping");
    recreateJson(jsonStr, pTokens, parser.toknext, 0);
    delete pTokens;
    return true;
}

#endif // CONFIG_MANAGER_RECREATE_JSON
