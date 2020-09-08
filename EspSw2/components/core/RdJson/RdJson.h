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

#pragma once
#include <stdlib.h>
#include <limits.h>
#include <WString.h>
#include "jsmnR.h"
#include <vector>

// Define this to enable reformatting of JSON
//#define RDJSON_RECREATE_JSON 1

class RdJson {
public:
    // Get location of element in JSON string
    static bool getElement(const char* dataPath,
                           int& startPos, int& strLen,
                           jsmntype_t& elemType, int& elemSize,
                           const char* pSourceStr);
    // Get a string from the JSON
    static String getString(const char* dataPath,
                            const char* defaultValue, bool& isValid,
                            jsmntype_t& elemType, int& elemSize,
                            const char* pSourceStr);

    // Alternate form of getString with fewer parameters
    static String getString(const char* dataPath, const char* defaultValue,
                            const char* pSourceStr, bool& isValid);

    // Alternate form of getString with fewer parameters
    static String getString(const char* dataPath, const char* defaultValue,
                            const char* pSourceStr);

    static double getDouble(const char* dataPath,
                            double defaultValue, bool& isValid,
                            const char* pSourceStr);

    static double getDouble(const char* dataPath, double defaultValue,
                            const char* pSourceStr);

    static long getLong(const char* dataPath,
                        long defaultValue, bool& isValid,
                        const char* pSourceStr);

    static long getLong(const char* dataPath, long defaultValue, const char* pSourceStr);

    static const char* getElemTypeStr(jsmntype_t type);

    static jsmntype_t getType(int& arrayLen, const char* pSourceStr);

    static const int MAX_KEYS_TO_RETURN = 100;
    static bool getKeys(const char *dataPath, std::vector<String>& keysVector, const char *pSourceStr);
    
    static bool getArrayElems(const char *dataPath, std::vector<String>& arrayElems, const char *pSourceStr);

    // Name value pair
    struct NameValuePair
    {
        String name;
        String value;
    };

    // Convert name value pairs to JSON
    static String getNameValuePairsJSON(std::vector<NameValuePair>& nameValuePairs, bool includeOuterBraces);

    static size_t safeStringLen(const char* pSrc,
                                bool skipJSONWhitespace = false, size_t maxx = LONG_MAX);

    static void safeStringCopy(char* pDest, const char* pSrc,
                               size_t maxx, bool skipJSONWhitespace = false);

    static void debugDumpParseResult(const char* pSourceStr, jsmntok_t* pTokens, int numTokens);

    static void escapeString(String& strToEsc);

    static void unescapeString(String& strToUnEsc);

    static int findKeyInJson(const char* jsonOriginal, jsmntok_t tokens[],
                             unsigned int numTokens, const char* dataPath,
                             int& endTokenIdx,
                             jsmntype_t keyType = JSMN_UNDEFINED);

    static const int RDJSON_MAX_TOKENS = 10000;
    static jsmntok_t* parseJson(const char* jsonStr, int& numTokens,
                                 int maxTokens = RDJSON_MAX_TOKENS);
    // Validate JSON
    static bool validateJson(const char* pSourceStr, int& numTokens);
    static char* safeStringDup(const char* pSrc, size_t maxx,
                               bool skipJSONWhitespace = false);
    static int findElemEnd(const char* jsonOriginal, jsmntok_t tokens[],
                             unsigned int numTokens, int startTokenIdx);
    static int findArrayElem(const char *jsonOriginal, jsmntok_t tokens[],
                          unsigned int numTokens, int startTokenIdx, 
                          int arrayElemIdx);

#ifdef RDJSON_RECREATE_JSON
    static int recreateJson(const char* js, jsmntok_t* t,
                            size_t count, int indent, String& outStr);
    static bool doPrint(const char* jsonStr);
#endif // RDJSON_RECREATE_JSON
};
