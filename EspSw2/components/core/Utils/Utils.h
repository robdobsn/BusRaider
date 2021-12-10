/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Utils
//
// Rob Dobson 2012-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <vector>
#include <Logger.h>
#include <stdio.h>

class Utils
{
public:

    // Test for a timeout handling wrap around
    // Usage example: isTimeout(millis(), myLastTime, 1000)
    // This will return true if myLastTime was set to millis() more than 1000 milliseconds ago
    static bool isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
    static bool isTimeout(uint64_t curTime, uint64_t lastTime, uint64_t maxDuration);

    // Get time until a timeout handling wrap around
    // Usage example: timeToTimeout(millis(), myLastTime)
    // Returns milliseconds since myLastTime
    static unsigned long timeToTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
    static uint64_t timeToTimeout(uint64_t curTime, uint64_t lastTime, uint64_t maxDuration);

    // Get time elapsed handling wrap around
    // Usage example: timeElapsed(millis(), myLastTime)
    static unsigned long timeElapsed(unsigned long curTime, unsigned long lastTime);
    static uint64_t timeElapsed(uint64_t curTime, uint64_t lastTime);

    // Setup a result string
    static void setJsonBoolResult(const char* req, String& resp, bool rslt, const char* otherJson = nullptr);

    // Set a result error
    static void setJsonErrorResult(const char* req, String& resp, const char* errorMsg, const char* otherJson = nullptr);

    // Set a JSON result
    static void setJsonResult(const char* pReq, String& resp, bool rslt, const char* errorMsg = nullptr, const char* otherJson = nullptr);

    // Following code from Unix sources
    static unsigned long convIPStrToAddr(String &inStr);

    // Escape a string in JSON
    static String escapeJSON(const String& inStr);

    // Convert HTTP query format to JSON
    // JSON only contains name/value pairs and not {}
    static String getJSONFromHTTPQueryStr(const char* inStr, bool mustStartWithQuestionMark = true);

    // Get Nth field from string
    static String getNthField(const char* inStr, int N, char separator);

    // Get a uint8_t value from the uint8_t pointer passed in
    // Increment the pointer (by 1)
    // Also checks endStop pointer value if provided
    static uint16_t getUint8AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal >= pEndStop)))
            return 0;
        uint8_t val = *pVal;
        pVal += 1;
        return val;
    }

    // Get an int8_t value from the uint8_t pointer passed in
    // Increment the pointer (by 1)
    // Also checks endStop pointer value if provided
    static int16_t getInt8AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal >= pEndStop)))
            return 0;
        int8_t val = *((int8_t*)pVal);
        pVal += 1;
        return val;
    }

    // Get a uint16_t little endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static uint16_t getLEUint16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 1 >= pEndStop)))
            return 0;
        uint16_t val = ((((uint16_t)(*(pVal+1))) * 256) + *pVal);
        pVal += 2;
        return val;
    }

    // Get a int16_t little endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static int16_t getLEInt16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 1 >= pEndStop)))
            return 0;
        uint32_t val = ((((uint32_t)(*(pVal+1))) * 256) + *pVal);
        pVal += 2;
        if (val <= 32767)
            return val;
        return val-65536;
    }

    // Get a uint16_t big endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static uint16_t getBEUint16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 1 >= pEndStop)))
            return 0;
        uint16_t val = ((((uint16_t)(*(pVal))) * 256) + *(pVal+1));
        pVal += 2;
        return val;
    }

    // Get a int16_t big endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static int16_t getBEInt16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 1 >= pEndStop)))
            return 0;
        uint32_t val = ((((uint32_t)(*(pVal))) * 256) + *(pVal+1));
        pVal += 2;
        if (val <= 32767)
            return val;
        return val-65536;
    }

    // Get a uint32_t big endian value from the uint8_t pointer passed in
    // Increment the pointer (by 4)
    // Also checks endStop pointer value if provided
    static uint32_t getBEUint32AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 3 >= pEndStop)))
            return 0;
        uint32_t val = ((((uint32_t)(*pVal)) << 24) + (((uint32_t)(*(pVal+1))) << 16) +
                        + (((uint32_t)(*(pVal+2))) << 8) + *(pVal+3));
        pVal += 4;
        return val;
    }

    // Get a float32_t big endian value from the uint8_t pointer passed in
    // Increment the pointer (by 4)
    // Also checks endStop pointer value if provided
    static float getBEfloat32AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = nullptr)
    {
        if (!pVal || (pEndStop && (pVal + 3 >= pEndStop)))
            return 0;
        
        float val = 0;
        uint8_t* pFloat = (uint8_t*)(&val)+3;
        for (int i = 0; i < 4; i++)
            *pFloat-- = *pVal++;
        return val;
    }

    // Set values into a buffer
    static void setBEInt8(uint8_t* pBuf, uint32_t offset, int8_t val)
    {
        if (!pBuf)
            return;
        *(pBuf+offset) = val;
    }

    // Set values into a buffer
    static void setBEUint8(uint8_t* pBuf, uint32_t offset, uint8_t val)
    {
        if (!pBuf)
            return;
        *(pBuf+offset) = val;
    }

    // Set values into a buffer
    static void setBEInt16(uint8_t* pBuf, uint32_t offset, int16_t val)
    {
        if (!pBuf)
            return;
        *(pBuf+offset) = (val >> 8) & 0x0ff;
        *(pBuf+offset+1) = val & 0xff;
    }

    // Set values into a buffer
    static void setBEUint16(uint8_t* pBuf, uint32_t offset, uint16_t val)
    {
        if (!pBuf)
            return;
        *(pBuf+offset) = (val >> 8) & 0x0ff;
        *(pBuf+offset+1) = val & 0xff;
    }

    // Set values into a buffer
    static void setBEUint32(uint8_t* pBuf, uint32_t offset, uint32_t val)
    {
        if (!pBuf)
            return;
        *(pBuf+offset) = (val >> 24) & 0x0ff;
        *(pBuf+offset+1) = (val >> 16) & 0x0ff;
        *(pBuf+offset+2) = (val >> 8) & 0x0ff;
        *(pBuf+offset+3) = val & 0xff;
    }

    // Set values into a buffer
    // Since ESP32 is little-endian this means reversing the order
    static void setBEFloat32(uint8_t* pBuf, uint32_t offset, float val)
    {
        if (!pBuf)
            return;
        uint8_t* pFloat = (uint8_t*)(&val)+3;
        uint8_t* pBufOff = pBuf + offset;
        for (int i = 0; i < 4; i++)
            *pBufOff++ = *pFloat--;
    }

    // Get a string from a fixed-length buffer
    // false if the string was truncated
    static bool strFromBuffer(const uint8_t* pBuf, uint32_t bufLen, String& outStr, bool asciiOnly=true);

    // Clamp - like std::clamp but for uint32_t
    static uint32_t clamp(uint32_t val, uint32_t lo, uint32_t hi)
    {
        if (val < lo)
            val = lo;
        if (val > hi)
            val = hi;
        return val;
    }

    // RGB Value
    struct RGBValue
    {
        RGBValue()
        {
            r=0; g=0; b=0;
        }
        RGBValue(uint32_t r_, uint32_t g_, uint32_t b_)
        {
            r=r_; g=g_; b=b_;
        }
        uint32_t r;
        uint32_t g;
        uint32_t b;
    };

    // Get RGB from hex string
    static RGBValue getRGBFromHex(const String& colourStr)
    {
        uint32_t colourRGB = strtoul(colourStr.c_str(), nullptr, 16);
        return RGBValue((colourRGB >> 16) & 0xff, (colourRGB >> 8) & 0xff, colourRGB & 0xff);
    }

    // Get decimal value of hex char
    static uint32_t getHexFromChar(int ch)
    {
        ch = toupper(ch);
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return 0;
    }

    // Extract bytes from hex encoded string
    static uint32_t getBytesFromHexStr(const char* inStr, uint8_t* outBuf, size_t maxOutBufLen);

    // Generate a hex string from bytes
    static void getHexStrFromBytes(const uint8_t* pBuf, uint32_t bufLen, String& outStr);
    
    // Generate a hex string from uint32_t
    static void getHexStrFromUint32(const uint32_t* pBuf, uint32_t bufLen, String& outStr, 
                const char* separator);
    
    // Find match in buffer (like strstr for unterminated strings)
    // Returns position in buffer of val or -1 if not found
    static int findInBuf(const uint8_t* pBuf, uint32_t bufLen, 
                const uint8_t* pToFind, uint32_t toFindLen)
    {
        for (uint32_t i = 0; i < bufLen; i++)
        {
            if (pBuf[i] == pToFind[0])
            {
                for (uint32_t j = 0; j < toFindLen; j++)
                {
                    uint32_t k = i + j;
                    if (k >= bufLen)
                        return -1;
                    if (pBuf[k] != pToFind[j])
                        break;
                    if (j == toFindLen-1)
                        return i;
                }
            }
        }
        return -1;
    }

    static void logHexBuf(const uint8_t* pBuf, uint32_t bufLen, const char* logPrefix, const char* logIntro)
    {
        if (!pBuf)
            return;
        // Output log string with prefix and length only if > 16 bytes
        if (bufLen > 16) {
            LOG_I(logPrefix, "%s len %d", logIntro, bufLen);
        }

        // Iterate over buffer
        uint32_t bufPos = 0;
        while (bufPos < bufLen)
        {
            char outBuf[400];
            strcpy(outBuf, "");
            char tmpBuf[10];
            uint32_t linePos = 0;
            while ((linePos < 16) && (bufPos < bufLen))
            {
                snprintf(tmpBuf, sizeof(tmpBuf), "%02x ", pBuf[bufPos]);
                strlcat(outBuf, tmpBuf, sizeof(outBuf));
                bufPos++;
                linePos++;
            }
            if (bufLen <= 16) {
                LOG_I(logPrefix, "%s len %d: %s", logIntro, bufLen, outBuf);
            } else {
                LOG_I(logPrefix, "%s", outBuf);
            }
        }
    }
};

// Name value pair double
struct NameValuePairDouble
{
    NameValuePairDouble(const char* itemName, double itemValue)
    {
        name = itemName;
        value = itemValue;
    }
    String name;
    double value;
};

// ABS macro
#define UTILS_ABS(N) ((N<0)?(-N):(N))
#define UTILS_MAX(A,B) (A > B ? A : B)
#define UTILS_MIN(A,B) (A < B ? A : B)


