// HDLC
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void MiniHDLCPutChFnType(uint8_t ch);
typedef void MiniHDLCFrameRxFnType(const uint8_t *framebuffer, int framelength);

// MiniHDLC
class MiniHDLC
{
  private:
    // If either of the following two octets appears in the transmitted data, an escape octet is sent,
    // followed by the original data octet with bit 5 inverted

    // The frame boundary octet is 01111110, (7E in hexadecimal notation)
    static constexpr uint8_t FRAME_BOUNDARY_OCTET = 0x7E;

    // Control escape octet", has the bit sequence '01111101', (7D hexadecimal) */
    static constexpr uint8_t CONTROL_ESCAPE_OCTET = 0x7D;

    // Invert octet explained above
    static constexpr uint8_t INVERT_OCTET = 0x20;

    // The frame check sequence (FCS) is a 16-bit CRC-CCITT
    // AVR Libc CRC function is _crc_ccitt_update()
    // Corresponding CRC function in Qt (www.qt.io) is qChecksum()
    static constexpr uint16_t CRC16_CCITT_INIT_VAL = 0xFFFF;

    // Max FRAME length
    static constexpr int MINIHDLC_MAX_FRAME_LENGTH = 5000;

    // CRC table
    static const uint16_t _CRCTable[256];

    // Callback functions for PutCh and FrameRx
    static MiniHDLCPutChFnType* _pPutChFn;
    static MiniHDLCFrameRxFnType* _pFrameRxFn;

    // Send FCS (CRC) big-endian - i.e. high byte first
    bool _bigEndian;

    // State vars
    int _framePos;
    uint16_t _frameCRC;
    bool _inEscapeSeq;

    // Receive buffer
    uint8_t _rxBuffer[MINIHDLC_MAX_FRAME_LENGTH + 1];

  private:
    uint16_t crcUpdateCCITT(unsigned short fcs, unsigned char value) 
    {
        return (fcs << 8) ^ _CRCTable[((fcs >> 8) ^ value) & 0xff];
    }

    void sendChar(uint8_t ch)
    {
        if (_pPutChFn)
            (*_pPutChFn)(ch);
    }

    void sendEscaped(uint8_t ch)
    {
        if ((ch == CONTROL_ESCAPE_OCTET) || (ch == FRAME_BOUNDARY_OCTET)) 
        {
            sendChar(CONTROL_ESCAPE_OCTET);
            ch ^= INVERT_OCTET;
        }
        sendChar(ch);
    }

  public:
    MiniHDLC(MiniHDLCPutChFnType* pPutChFn, MiniHDLCFrameRxFnType* pFrameRxFn, bool bigEndian)
    {
         _pPutChFn = pPutChFn;
        _pFrameRxFn = pFrameRxFn;
        _framePos = 0;
        _frameCRC = CRC16_CCITT_INIT_VAL;
        _inEscapeSeq = false;
        _bigEndian = bigEndian;
    }

    // Called by external function that has data to process
    void handleChar(uint8_t ch);

    // Called to send a frame
    void sendFrame(const uint8_t *pData, int frameLen);

};
