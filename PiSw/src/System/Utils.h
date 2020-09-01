// Utils
// Rob Dobson 2012-2017

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "rdutils.h"
#include "string.h"
#include "logging.h"
#include "ee_sprintf.h"

class Utils
{
public:

    // Get a uint8_t value from the uint8_t pointer passed in
    // Increment the pointer (by 1)
    // Also checks endStop pointer value if provided
    static uint16_t getUint8AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal >= pEndStop))
            return 0;
        uint8_t val = *pVal;
        pVal += 1;
        return val;
    }

    // Get an int8_t value from the uint8_t pointer passed in
    // Increment the pointer (by 1)
    // Also checks endStop pointer value if provided
    static int16_t getInt8AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal >= pEndStop))
            return 0;
        int8_t val = *((int8_t*)pVal);
        pVal += 1;
        return val;
    }

    // Get a uint16_t little endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static uint16_t getLEUint16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal + 1 >= pEndStop))
            return 0;
        uint16_t val = ((((uint16_t)(*(pVal+1))) * 256) + *pVal);
        pVal += 2;
        return val;
    }

    // Get a int16_t little endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static int16_t getLEInt16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal + 1 >= pEndStop))
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
    static uint16_t getBEUint16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal + 1 >= pEndStop))
            return 0;
        uint16_t val = ((((uint16_t)(*(pVal))) * 256) + *(pVal+1));
        pVal += 2;
        return val;
    }

    // Get a int16_t big endian value from the uint8_t pointer passed in
    // Increment the pointer (by 2)
    // Also checks endStop pointer value if provided
    static int16_t getBEInt16AndInc(const uint8_t*& pVal, const uint8_t* pEndStop = NULL)
    {
        if (pEndStop && (pVal + 1 >= pEndStop))
            return 0;
        uint32_t val = ((((uint32_t)(*(pVal))) * 256) + *(pVal+1));
        pVal += 2;
        if (val <= 32767)
            return val;
        return val-65536;
    }

    // Set values into a buffer
    static void setInt8(uint8_t* pBuf, uint32_t offset, int8_t val)
    {
        *(pBuf+offset) = val;
    }

    // Set values into a buffer
    static void setUint8(uint8_t* pBuf, uint32_t offset, uint8_t val)
    {
        *(pBuf+offset) = val;
    }

    // Set values into a buffer
    static void setUint8AndInc(uint8_t* pBuf, uint32_t& offset, uint8_t val)
    {
        *(pBuf+offset) = val;
        offset++;
    }

    // Set values into a buffer
    static void setBEInt16(uint8_t* pBuf, uint32_t offset, int16_t val)
    {
        *(pBuf+offset) = (val >> 8) & 0x0ff;
        *(pBuf+offset+1) = val & 0xff;
    }

    // Set values into a buffer
    static void setBEUint16(uint8_t* pBuf, uint32_t offset, uint16_t val)
    {
        *(pBuf+offset) = (val >> 8) & 0x0ff;
        *(pBuf+offset+1) = val & 0xff;
    }

    // Set values into a buffer
    static void setLEInt16AndInc(uint8_t* pBuf, uint32_t& offset, int16_t val)
    {
        *(pBuf+offset+1) = (val >> 8) & 0x0ff;
        *(pBuf+offset) = val & 0xff;
        offset+=2;
    }

    // Set values into a buffer
    static void setLEUint16AndInc(uint8_t* pBuf, uint32_t& offset, uint16_t val)
    {
        *(pBuf+offset+1) = (val >> 8) & 0x0ff;
        *(pBuf+offset) = val & 0xff;
        offset+=2;
    }

    // Set values into a buffer
    static void setLEUint32AndInc(uint8_t* pBuf, uint32_t& offset, uint32_t val)
    {
        *(pBuf+offset+3) = (val >> 24) & 0x0ff;
        *(pBuf+offset+2) = (val >> 16) & 0x0ff;
        *(pBuf+offset+1) = (val >> 8) & 0x0ff;
        *(pBuf+offset) = val & 0xff;
        offset+=4;
    }

    // Set values into a buffer
    // Since ESP32 is little-endian this means reversing the order
    static void setBEFloat32(uint8_t* pBuf, uint32_t offset, float val)
    {
        uint8_t* pFloat = (uint8_t*)(&val)+3;
        uint8_t* pBufOff = pBuf + offset;
        for (int i = 0; i < 4; i++)
            *pBufOff++ = *pFloat--;
    }

    // Get a string from a fixed-length buffer
    // false if the string was truncated
    static bool strFromBuffer(const uint8_t* pBuf, uint32_t bufLen, char* outStr, 
            uint32_t outStrMaxLen, bool asciiOnly=true);

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

    // Get decimal value of hex char
    static uint32_t getHexFromChar(int ch)
    {
        ch = rdtoupper(ch);
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return 0;
    }

    // Extract bytes from hex encoded string
    static uint32_t getBytesFromHexStr(const char* inStr, uint8_t* outBuf, size_t maxOutBufLen);

    // Generate a hex string from bytes
    static void getHexStrFromBytes(const uint8_t* pBuf, uint32_t bufLen, char* outStr, uint32_t outStrMaxLen);

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

    static void logHexBuf(const uint8_t* pBuf, uint32_t bufLen, const char* logPrefix, 
            unsigned int logLevel, const char* logIntro)
    {
        // Output log string with prefix and length only if > 16 bytes
        if (bufLen > 16)
            LogWrite(logPrefix, logLevel, "%s len %d", logIntro, bufLen);

        // Iterate over buffer
        uint32_t bufPos = 0;
        while (bufPos < bufLen)
        {
            char outBuf[200];
            strcpy(outBuf, "");
            char tmpBuf[10];
            uint32_t linePos = 0;
            while ((linePos < 16) && (bufPos < bufLen))
            {
                ee_sprintf(tmpBuf, "%02x ", pBuf[bufPos]);
                strlcat(outBuf, tmpBuf, sizeof(outBuf));
                bufPos++;
                linePos++;
            }
            if (bufLen <= 16)
                LogWrite(logPrefix, logLevel, "%s len %d: %s", logIntro, bufLen, outBuf);
            else
                LogWrite(logPrefix, logLevel, "%s", outBuf);
        }
    }
};

