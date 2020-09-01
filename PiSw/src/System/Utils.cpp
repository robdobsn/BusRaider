// Utils
// Rob Dobson 2012-2020

#include "Utils.h"
#include <limits.h>

#ifndef INADDR_NONE
#define INADDR_NONE         ((uint32_t)0xffffffffUL)
#endif

// static const char* MODULE_PREFIX = "Utils";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a string from a fixed-length buffer
// false if the string was truncated
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Utils::strFromBuffer(const uint8_t* pBuf, uint32_t bufLen, char* outStr, 
            uint32_t outStrMaxLen, bool asciiOnly)
{
    // Result max len
    uint32_t lenToCopy = (bufLen < outStrMaxLen ? bufLen : outStrMaxLen);

    // Extract string
    char* pOut = outStr;
    for (uint32_t i = 0; i < lenToCopy; i++)
    {
        if ((pBuf[i] == 0) || (asciiOnly && (pBuf[i] > 127)))
            break;
        *pOut++ = pBuf[i];
    }
    *pOut = 0;
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

void Utils::getHexStrFromBytes(const uint8_t* pBuf, uint32_t bufLen, char* outStr, uint32_t outStrMaxLen)
{
    // Generate hex
    char* pOut = outStr;
    uint32_t outPos = 0;
    *pOut = '\0';
    for (uint32_t i = 0; i < bufLen; i++)
    {
        if (outPos + 2 >= outStrMaxLen)
            break;
        ee_sprintf(pOut, "%02x", pBuf[i]);
        pOut += 2;
        outPos += 2;
    }
}

