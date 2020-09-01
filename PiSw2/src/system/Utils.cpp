// Utils
// Rob Dobson 2012-2017

#include "Utils.h"
#include <string.h>

#if !defined(ULONG_LONG_MAX)
#define ULONG_LONG_MAX 0xffffffffffffffff
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timeout test
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

