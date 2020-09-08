/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolOverAscii
// Encode and decode OverASCII protocol
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OVERASCII_USE_STD_FUNCTION_AND_BIND 1

#ifdef OVERASCII_USE_STD_FUNCTION_AND_BIND
#include <functional>
// Received frame callback function type
typedef std::function<void(const uint8_t *framebuffer, int framelength)> OverAsciiFrameFnType;
#else
typedef void (*OverAsciiFrameFnType)(const uint8_t *framebuffer, int framelength);
#endif

class ProtocolOverAscii
{
public:
    // Constructor
    ProtocolOverAscii();

    // Destructor
    virtual ~ProtocolOverAscii();

    // Clear state
    void clear();
    
    // Process char to decode
    // Returns -1 if char was an escape code - remainder will be returned later
    int decodeByte(uint8_t ch);

    // Called to encode a frame
    // Returns encoded length
    uint32_t encodeFrame(const uint8_t *pData, uint32_t frameLen, 
                uint8_t* pEncoded, uint32_t encodedMaxLen);

private:
    // Control escape octet - chosen to be an infrequently used ASCII 
    // control values in the range 0x00-0x0F
    // with MSB set
    static constexpr uint8_t OVERASCII_ESCAPE_1 = 0x85;
    static constexpr uint8_t OVERASCII_ESCAPE_2 = 0x8E;
    static constexpr uint8_t OVERASCII_ESCAPE_3 = 0x8F;

    // Code used to modify value by XOR
    static constexpr uint8_t OVERASCII_MOD_CODE = 0x20;

    // State vars
    uint8_t _escapeSeqCode;

    // Helpers
    uint32_t addBytes(uint8_t* pEncoded, uint32_t encodedMaxLen, uint32_t bufPos, uint32_t escCode, uint32_t byteCode);
    uint32_t addByte(uint8_t* pEncoded, uint32_t encodedMaxLen, uint32_t bufPos, uint32_t byteCode);
};
