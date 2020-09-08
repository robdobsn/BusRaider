/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolOverAscii
// Encode and decode OverASCII protocol
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ProtocolOverAscii.h"
#include "string.h"

// #define DEBUG_OVERASCII

// #ifdef DEBUG_OVERASCII
// #include <Logger.h>
// static const char* MODULE_PREFIX = "OverAscii";
// #endif

// Constructor for HDLC with frame-wise transmit
ProtocolOverAscii::ProtocolOverAscii()
{
    clear();
}

// Destructor
ProtocolOverAscii::~ProtocolOverAscii()
{
}

// Clear state
void ProtocolOverAscii::clear()
{
    _escapeSeqCode = 0;
}

// Function to handle char for decoding
// Returns -1 if char was an escape code - remainder will be returned later
int ProtocolOverAscii::decodeByte(uint8_t ch)
{
    // Check if in escape sequence
    if (_escapeSeqCode != 0)
    {
        uint32_t prevEscCode = _escapeSeqCode;
        _escapeSeqCode = 0;
        if (prevEscCode == 1)
            return (ch ^ OVERASCII_MOD_CODE) & 0x7f;
        else if (prevEscCode == 2)
            return ch ^ OVERASCII_MOD_CODE;
        return ch;
    }
    else if (ch == OVERASCII_ESCAPE_1)
    {
        _escapeSeqCode = 1;
        return -1;
    }
    else if (ch == OVERASCII_ESCAPE_2)
    {
        _escapeSeqCode = 2;
        return -1;
    }
    else if (ch == OVERASCII_ESCAPE_3)
    {
        _escapeSeqCode = 3;
        return -1;
    }
    else
    {
        return ch & 0x7f;
    }
}

// Encode frame
// Values 0x00-0x0F map to ESCAPE_CODE_1, VALUE_XOR_20H_AND_MSB_SET
// Values 0x10-0x7f map to VALUE_WITH_MSB_SET
// Values 0x80-0x8F map to ESCAPE_CODE_2, VALUE_XOR_20H
// Values 0x90-0xff map to ESCAPE_CODE_3, VALUE

// Value ESCAPE_CODE_1 maps to ESCAPE_CODE_1 + VALUE 
uint32_t ProtocolOverAscii::encodeFrame(const uint8_t *pData, uint32_t frameLen, 
                uint8_t* pEncoded, uint32_t encodedMaxLen)
{
    // Iterate over frame
    uint32_t bufPos = 0;
    for (uint32_t byIdx = 0; byIdx < frameLen; byIdx++)
    {
        // To encode
        uint32_t toEnc = pData[byIdx];

        // Handle escape char
        if (toEnc <= 0x0f)
        {
            bufPos = addBytes(pEncoded, encodedMaxLen, bufPos, OVERASCII_ESCAPE_1, (toEnc ^ OVERASCII_MOD_CODE) | 0x80);
        }
        else if ((toEnc >= 0x10) && (toEnc <= 0x7f))
        {
            bufPos = addByte(pEncoded, encodedMaxLen, bufPos, toEnc | 0x80);
        }
        else if ((toEnc >= 0x80) && (toEnc <= 0x8f))
        {
            bufPos = addBytes(pEncoded, encodedMaxLen, bufPos, OVERASCII_ESCAPE_2, toEnc ^ OVERASCII_MOD_CODE);
        }
        else if ((toEnc >= 0x90) && (toEnc <= 0xff))
        {
            bufPos = addBytes(pEncoded, encodedMaxLen, bufPos, OVERASCII_ESCAPE_3, toEnc);
        }
    }

    return bufPos;
}

uint32_t ProtocolOverAscii::addBytes(uint8_t* pEncoded, uint32_t encodedMaxLen, uint32_t bufPos, uint32_t escCode, uint32_t byteCode)
{
    bufPos = addByte(pEncoded, encodedMaxLen, bufPos, escCode);
    return addByte(pEncoded, encodedMaxLen, bufPos, byteCode);
}

uint32_t ProtocolOverAscii::addByte(uint8_t* pEncoded, uint32_t encodedMaxLen, uint32_t bufPos, uint32_t byteCode)
{
    // Check valid
    if (bufPos >= encodedMaxLen)
        return bufPos;

    // Add to buffer
    pEncoded[bufPos++] = byteCode;
    return bufPos;
}

