/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit tests of ProtocolOverAscii
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ProtocolOverAscii.h>
#include <Utils.h>
#include <Logger.h>
#include "unity.h"

// #define DEBUG_OVERASCII_UNIT_TEST
#if defined(DEBUG_OVERASCII_UNIT_TEST)
static const char* MODULE_PREFIX = "OverAsciiTest";
#endif

TEST_CASE("test_overAscii", "[ProtocolOverAscii]")
{
    // Debug
#ifdef DEBUG_OVERASCII_UNIT_TEST
    LOG_I(MODULE_PREFIX, "ProtocolOverAsciis Test");
#endif

    // Encode
    static const uint8_t testFrame1[] = { 0x20, 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x05, 0x0e, 0x0f, 0x85, 0x8e, 0x8f, 0x90, 0xff, 
                0x22, 0x0e, 0x45, 0x85, 0xf2, 0x5f, 0x3f, 0x0e, 0x92, 0x51, 0xda, 0x4c, 0x9b, 0xa5, 0x8e, 0x91, 0x05, 0x0f, 0x1d };
#ifdef DEBUG_OVERASCII_UNIT_TEST
    Utils::logHexBuf(testFrame1, sizeof(testFrame1), MODULE_PREFIX, "ProtocolOverAscii original");
#endif
    uint8_t outFrame[500];
    ProtocolOverAscii testPt;
    uint32_t outLen = testPt.encodeFrame(testFrame1, sizeof(testFrame1), outFrame, sizeof(outFrame));
#ifdef DEBUG_OVERASCII_UNIT_TEST
    Utils::logHexBuf(outFrame, outLen, MODULE_PREFIX, "ProtocolOverAscii encoded");
#endif

    // Decode
    testPt.clear();
    uint8_t decodedFrame[500];
    uint32_t decodePos = 0;
    for (uint32_t i = 0; i < outLen; i++)
    {
        int val = testPt.decodeByte(outFrame[i]);
        if (val >= 0)
        {
            if (decodePos < sizeof(decodedFrame))
            {
                decodedFrame[decodePos++] = val;
            }
        }
    }

    // Check correct length
#ifdef DEBUG_OVERASCII_UNIT_TEST
    Utils::logHexBuf(decodedFrame, decodePos, MODULE_PREFIX, "ProtocolOverAscii decoded");
#endif
    TEST_ASSERT_MESSAGE (decodePos == sizeof(testFrame1), "OverAscii decoded length differs");

    // Check valid
    if (decodePos == sizeof(testFrame1))
    {
        bool isOk = true;
        for (uint32_t i = 0; i < sizeof(testFrame1); i++)
        {
            if (testFrame1[i] != decodedFrame[i])
            {
                isOk = false;
            }
        }
        TEST_ASSERT_MESSAGE (isOk, "OverAscii decoded differs");
    }
}
