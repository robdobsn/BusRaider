// Utils
// Rob Dobson 2012-2017

#include <Utils.h>
#include <limits.h>
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Logger.h>

#ifndef INADDR_NONE
#define INADDR_NONE         ((uint32_t)0xffffffffUL)
#endif

static const char* MODULE_PREFIX = "Utils";

// #define DEBUG_EXTRACT_NAME_VALUES

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timeouts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Utils::isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime)
    {
        return (curTime > lastTime + maxDuration);
    }
    return (ULONG_MAX - (lastTime - curTime) > maxDuration);
}

bool Utils::isTimeout(uint64_t curTime, uint64_t lastTime, uint64_t maxDuration)
{
    if (curTime >= lastTime)
    {
        return (curTime > lastTime + maxDuration);
    }
    return (ULONG_LONG_MAX - (lastTime - curTime) > maxDuration);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Time before time-out occurs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Time to timeout handling counter wrapping
unsigned long Utils::timeToTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime)
    {
        if (curTime > lastTime + maxDuration)
        {
            return 0;
        }
        return maxDuration - (curTime - lastTime);
    }
    unsigned long wrapTime = ULONG_MAX - (lastTime - curTime);
    if (wrapTime > maxDuration)
    {
        return 0;
    }
    return maxDuration - wrapTime;
}

uint64_t Utils::timeToTimeout(uint64_t curTime, uint64_t lastTime, uint64_t maxDuration)
{
    if (curTime >= lastTime)
    {
        if (curTime > lastTime + maxDuration)
        {
            return 0;
        }
        return maxDuration - (curTime - lastTime);
    }
    if (ULONG_LONG_MAX - (lastTime - curTime) > maxDuration)
    {
        return 0;
    }
    return maxDuration - (ULONG_LONG_MAX - (lastTime - curTime));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Elapsed time handling counter wrapping
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long Utils::timeElapsed(unsigned long curTime, unsigned long lastTime)
{
    if (curTime >= lastTime)
        return curTime - lastTime;
    return (ULONG_MAX - lastTime) + 1 + curTime;
}

uint64_t Utils::timeElapsed(uint64_t curTime, uint64_t lastTime)
{
    if (curTime >= lastTime)
        return curTime - lastTime;
    return (ULONG_LONG_MAX - lastTime) + 1 + curTime;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set results for JSON communications
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Utils::setJsonBoolResult(const char* pReq, String& resp, bool rslt, const char* otherJson)
{
    String additionalJson = "";
    if ((otherJson) && (strlen(otherJson) > 0))
        additionalJson = otherJson + String(",");
    String retStr;
    resp = "{\"req\":\"" + String(pReq) + "\"," + additionalJson + "\"rslt\":";
    if (rslt)
        resp += "\"ok\"}";
    else
        resp += "\"fail\"}";
}

// Set a result error
void Utils::setJsonErrorResult(const char* pReq, String& resp, const char* errorMsg, const char* otherJson)
{
    String additionalJson = "";
    if ((otherJson) && (strlen(otherJson) > 0))
        additionalJson = otherJson + String(",");
    String retStr;
    String errorMsgStr;
    if (errorMsg)
        errorMsgStr = errorMsg;
    resp = "{\"req\":\"" + String(pReq) + "\"," + additionalJson + "\"rslt\":\"fail\",\"error\":\"" + errorMsgStr + "\"}";
}

// Set result
void Utils::setJsonResult(const char* pReq, String& resp, bool rslt, const char* errorMsg, const char* otherJson)
{
    if (rslt)
        setJsonBoolResult(pReq, resp, rslt, otherJson);
    else
        setJsonErrorResult(pReq, resp, errorMsg, otherJson);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Escape JSON string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String Utils::escapeJSON(const String& inStr)
{
    String outStr;
    // Reserve a bit more than the inStr length
    outStr.reserve((inStr.length() * 3) / 2);
    // Replace chars with escapes as needed
    for (unsigned int i = 0; i < inStr.length(); i++) 
    {
        int c = inStr.charAt(i);
        if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) 
        {
            outStr += "\\u";
            String cx = String(c, HEX);
            for (unsigned int j = 0; j < 4-cx.length(); j++)
                outStr += "0";
            outStr += cx;
        } 
        else
        {
            outStr += (char)c;
        }
    }
    return outStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert HTTP query format to JSON
// JSON only contains name/value pairs and not {}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String Utils::getJSONFromHTTPQueryStr(const char* inStr, bool mustStartWithQuestionMark)
{
    String outStr;
    outStr.reserve((strlen(inStr) * 3) / 2);
    const char* pStr = inStr;
    bool isActive = !mustStartWithQuestionMark;
    String curName;
    String curVal;
    bool inValue = false;
    while (*pStr)
    {
        // Handle start of query
        if (!isActive)
        {
            if (*pStr != '?')
            {
                pStr++;
                continue;
            }
            isActive = true;
        }
        
        // Ignore ? symbol
        if (*pStr == '?')
        {
            pStr++;
            continue;
        }

        // Check for separator values
        if (*pStr == '=')
        {
            inValue = true;
            curVal = "";
            pStr++;
            continue;
        }

        // Check for delimiter
        if (*pStr == '&')
        {
            // Store value
            if (inValue && (curName.length() > 0))
            {
                if (outStr.length() > 0)
                    outStr += ",";
                outStr += "\"" + curName + "\":" + "\"" + curVal + "\"";
            }
            inValue = false;
            curName = "";
            pStr++;
            continue;
        }

        // Form name or value
        if (inValue)
            curVal += *pStr;
        else
            curName += *pStr;
        pStr++;
    }

    // Finish up
    if (inValue && (curName.length() > 0))
    {
        if (outStr.length() > 0)
            outStr += ",";
        outStr += "\"" + curName + "\":" + "\"" + curVal + "\"";
    }
    return outStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Nth field from delimited string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String Utils::getNthField(const char* inStr, int N, char separator)
{
	String retStr;
	// Find separators
	const char* pStr = inStr;
	int len = -1;
	const char* pField = NULL;
	int sepIdx = 0;
	// Find the field and length
	while (true)
	{
		// Check if this is the start of the field we want
		if ((sepIdx == N) && (*pStr != '\0') && (pField == NULL))
			pField = pStr;
		// Check for separator (or end)
		if ((*pStr == separator) || (*pStr == '\0'))
			sepIdx++;
		// See if we have found the end point of the field
		if (sepIdx == N + 1)
		{
			len = pStr - pField;
			break;
		}
		// End?
		if (*pStr == '\0')
			break;
		// Bump
		pStr++;
	}
	// Return if invalid
	if ((pField == NULL) || (len == -1))
		return retStr;
	// Create buffer for the string
	char* pTmpStr = new char[len + 1];
	if (!pTmpStr)
		return retStr;
	memcpy(pTmpStr, pField, len);
	pTmpStr[len] = 0;
	retStr = pTmpStr;
	delete[] pTmpStr;
	return retStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a string from a fixed-length buffer
// false if the string was truncated
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Utils::strFromBuffer(const uint8_t* pBuf, uint32_t bufLen, String& outStr, bool asciiOnly)
{
    // Handling on stack or heap?
    static const uint32_t STR_FROM_BUFFER_STACK_MAXLEN = 250;
    static const uint32_t STR_FROM_BUFFER_MAXLEN = 5000;
    uint32_t lenToCopy = (bufLen < STR_FROM_BUFFER_MAXLEN ? bufLen : STR_FROM_BUFFER_MAXLEN);

    // Check if we can use stack
    if (lenToCopy <= STR_FROM_BUFFER_STACK_MAXLEN)
    {
        char tmpStr[lenToCopy+1];
        char* pOut = tmpStr;
        for (int i = 0; i < lenToCopy; i++)
        {
            if ((pBuf[i] == 0) || (asciiOnly && (pBuf[i] > 127)))
                break;
            *pOut++ = pBuf[i];
        }
        *pOut = 0;
        outStr = tmpStr;
        return lenToCopy == bufLen;
    }

    // Use heap
    char* tmpStr = new char[lenToCopy+1];
    if (!tmpStr)
    {
        outStr = "";
        return false;
    }
    char* pOut = tmpStr;
    for (int i = 0; i < lenToCopy; i++)
    {
        if ((pBuf[i] == 0) || (asciiOnly && (pBuf[i] > 127)))
            break;
        *pOut++ = pBuf[i];
    }
    *pOut = 0;
    outStr = tmpStr;
    delete [] tmpStr;
    return lenToCopy == bufLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get bytes from hex-encoded string
// Assumes no space between hex digits (e.g. 55aa55aa, etc)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t Utils::getBytesFromHexStr(const char* inStr, uint8_t* outBuf, size_t maxOutBufLen)
{
    // Mapping ASCII to hex
    static const uint8_t chToNyb[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // 01234567
        0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 89:;<=>?
        0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, // @ABCDEFG
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HIJKLMNO
    };

    // Clear initially
    uint32_t numBytes = maxOutBufLen < strlen(inStr) / 2 ? maxOutBufLen : strlen(inStr) / 2;
    uint32_t posIdx = 0;
    for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx++)
    {
        uint32_t nyb0Idx = (inStr[posIdx++] & 0x1F) ^ 0x10;
        uint32_t nyb1Idx = (inStr[posIdx++] & 0x1F) ^ 0x10;
        outBuf[byteIdx] = (chToNyb[nyb0Idx] << 4) + chToNyb[nyb1Idx];
    };
    return numBytes;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generate a hex string from bytes
// Generates no space between hex digits (e.g. 55aa55aa, etc)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Utils::getHexStrFromBytes(const uint8_t* pBuf, uint32_t bufLen, String& outStr)
{
    // Setup outStr
    outStr = "";
    outStr.reserve(bufLen * 2);

    // Generate hex
    for (uint32_t i = 0; i < bufLen; i++)
    {
        char tmpStr[10];
        snprintf(tmpStr, sizeof(tmpStr), "%02x", pBuf[i]);
        outStr += tmpStr;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert IP string to IP address bytes
// This code from Unix sources
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long Utils::convIPStrToAddr(String &inStr)
{
    char buf[30];
    char *cp = buf;

    inStr.toCharArray(cp, 29);
    unsigned long val, base, n;
    char c;
    unsigned long parts[4], *pp = parts;

    for (;;)
    {
        /*
            * Collect number up to ``.''.
            * Values are specified as for C:
            * 0x=hex, 0=octal, other=decimal.
            */
        val = 0;
        base = 10;
        if (*cp == '0')
        {
            if ((*++cp == 'x') || (*cp == 'X'))
            {
                base = 16, cp++;
            }
            else
            {
                base = 8;
            }
        }
        while ((c = *cp) != '\0')
        {
            if (isascii(c) && isdigit(c))
            {
                val = (val * base) + (c - '0');
                cp++;
                continue;
            }
            if ((base == 16) && isascii(c) && isxdigit(c))
            {
                val = (val << 4) +
                        (c + 10 - (islower(c) ? 'a' : 'A'));
                cp++;
                continue;
            }
            break;
        }
        if (*cp == '.')
        {
            /*
                * Internet format:
                *  a.b.c.d
                *  a.b.c (with c treated as 16-bits)
                *  a.b (with b treated as 24 bits)
                */
            if ((pp >= parts + 3) || (val > 0xff))
            {
                return (INADDR_NONE);
            }
            *pp++ = val, cp++;
        }
        else
        {
            break;
        }
    }

    /*
        * Check for trailing characters.
        */
    if (*cp && (!isascii(*cp) || !isspace(*cp)))
    {
        return (INADDR_NONE);
    }

    /*
        * Concoct the address according to
        * the number of parts specified.
        */
    n = pp - parts + 1;
    switch (n)
    {
    case 1: /* a -- 32 bits */
        break;

    case 2: /* a.b -- 8.24 bits */
        if (val > 0xffffff)
        {
            return (INADDR_NONE);
        }
        val |= parts[0] << 24;
        break;

    case 3: /* a.b.c -- 8.8.16 bits */
        if (val > 0xffff)
        {
            return (INADDR_NONE);
        }
        val |= (parts[0] << 24) | (parts[1] << 16);
        break;

    case 4: /* a.b.c.d -- 8.8.8.8 bits */
        if (val > 0xff)
        {
            return (INADDR_NONE);
        }
        val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
        break;
    }
    return (val);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extract name-value pairs from string
// nameValueSep - e.g. "=" for HTTP
// pairDelim - e.g. "&" for HTTP
// pairDelimAlt - e.g. ";" for HTTP alternate (pass 0 if not needed)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Utils::extractNameValues(const String& inStr, 
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
// Low-level time functions for Arduino compat
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint64_t IRAM_ATTR micros()
{
    return (uint64_t) (esp_timer_get_time());
}

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

void IRAM_ATTR delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(uint64_t us)
{
    uint64_t m = micros();
    if(us){
        int64_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                __asm__ volatile ("nop");
            }
        }
        while(micros() < e){
            __asm__ volatile ("nop");
        }
    }
}
