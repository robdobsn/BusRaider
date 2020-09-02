/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HDLC Bit and Bytewise
// This HDLC implementation doesn't completely conform to HDLC
// Currently STX and ETX are not sent
// There is no flow control
// Bit and Byte oriented HDLC is supported with appropriate bit/byte stuffing
// Very loosely based on https://github.com/mengguang/minihdlc
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USE_STD_FUNCTION_AND_BIND 1

#ifdef USE_STD_FUNCTION_AND_BIND
#include <functional>
// Put byte or bit callback function type
typedef std::function<void(uint8_t ch)> MiniHDLCPutChFnType;
// Received frame callback function type
typedef std::function<void(const uint8_t *framebuffer, unsigned framelength)> MiniHDLCFrameFnType;
#else
typedef void (*MiniHDLCPutChFnType)(uint8_t ch);
typedef void (*MiniHDLCFrameFnType)(const uint8_t *framebuffer, unsigned framelength);
#endif

class MiniHDLCStats
{
public:
    MiniHDLCStats()
    {
        clear();
    }
    void clear()
    {
        _rxFrameCount = 0;
        _frameCRCErrCount = 0;
        _frameTooLongCount = 0;
        _rxBufAllocFail = 0;
    }
    uint32_t _rxFrameCount;
    uint32_t _frameCRCErrCount;
    uint32_t _frameTooLongCount;
    uint32_t _rxBufAllocFail;
};

// MiniHDLC
class MiniHDLC
{
public:
    // Constructor for HDLC with bit/bytewise transmit
    // If bitwise HDLC then the first parameter will receive bits not bytes 
    MiniHDLC(uint32_t rxMsgMaxLen, MiniHDLCPutChFnType putChFn, MiniHDLCFrameFnType frameRxFn,
				bool bigEndianCRC = true, bool bitwiseHDLC = false);

    // Constructor for HDLC with frame-wise transmit
    MiniHDLC(MiniHDLCFrameFnType frameTxFn, MiniHDLCFrameFnType frameRxFn,
            uint32_t txMsgMaxLen=1000, uint32_t rxMsgMaxLen=1000, bool bigEndianCRC = true, bool bitwiseHDLC = false);

    // Destructor
    virtual ~MiniHDLC();
    
    // Called by external function that has byte-wise data to process
    void handleChar(uint8_t ch);
    void handleBuffer(const uint8_t* pBuf, unsigned numBytes);

    // Called by external function that has bit-wise data to process
    void handleBit(uint8_t bit);

    // Called to send a frame
    void sendFrame(const uint8_t *pData, unsigned frameLen);

    // Set frame rx max length
    void setFrameRxMaxLen(uint32_t rxMaxLen);

    // Get frame rx max len
    uint32_t getFrameRxMaxLen()
    {
        return _rxBufferMaxLen;
    }

    // Get frame tx buffer
    uint8_t* getFrameTxBuf()
    {
        return _pTxBuffer;
    }

    // Get frame tx len
    uint32_t getFrameTxLen()
    {
        return _txBufferPos;
    }

    // Get stats
    MiniHDLCStats* getStats()
    {
        return &_stats;
    }

    // Compute CCITT CRC16
    static unsigned computeCRC16(const uint8_t* pData, unsigned len);

private:
    // If either of the following two octets appears in the transmitted data, an escape octet is sent,
    // followed by the original data octet with bit 5 inverted

    // The frame boundary octet - changed from classic HDLC to move out of ascii range
    static constexpr uint8_t FRAME_BOUNDARY_OCTET = 0xE7;

    // Control escape octet - changed from classic HDLC to move out of ascii range
    static constexpr uint8_t CONTROL_ESCAPE_OCTET = 0xD7;

    // Invert octet explained above
    static constexpr uint8_t INVERT_OCTET = 0x20;

    // The frame check sequence (FCS) is a 16-bit CRC-CCITT
    // AVR Libc CRC function is _crc_ccitt_update()
    // Corresponding CRC function in Qt (www.qt.io) is qChecksum()
    static constexpr uint16_t CRC16_CCITT_INIT_VAL = 0xFFFF;

    // CRC table
    static const uint16_t _CRCTable[256];

    // Callback functions for PutCh/PutBit and FrameRx
#ifdef USE_STD_FUNCTION_AND_BIND
    MiniHDLCPutChFnType _putChFn;
    MiniHDLCFrameFnType _frameRxFn;
    MiniHDLCFrameFnType _frameTxFn;
#else
    static MiniHDLCPutChFnType _putChFn;
    static MiniHDLCFrameFnType _frameRxFn;
    static MiniHDLCFrameFnType _frameTxFn;
#endif

    // Bitwise HDLC flag (otherwise byte-wise)
    bool _bitwiseHDLC;

    // Send FCS (CRC) big-endian - i.e. high byte first
    bool _bigEndianCRC;

    // State vars
    int _framePos;
    uint16_t _frameCRC;
    bool _inEscapeSeq;

    // Bitwise state
    uint8_t _bitwiseLast8Bits;
    uint8_t _bitwiseByte;
    int _bitwiseBitCount;
    int _bitwiseSendOnesCount;

    // Receive buffer
    uint8_t* _pRxBuffer;
    uint32_t _rxBufferMaxLen;
    uint32_t _rxBufferAllocLen;
    static const uint32_t RX_BUFFER_MIN_ALLOC = 1024;
    static const uint32_t RX_BUFFER_ALLOC_INC = 2048;

    // Transmit buffer
    uint32_t _txBufferMaxLen;
    uint8_t* _pTxBuffer;
    uint32_t _txBufferPos;
    uint32_t _txBufferBitPos;

    // Stats
    MiniHDLCStats _stats;

private:
    static uint16_t crcUpdateCCITT(unsigned short fcs, unsigned char value);
    void sendChar(uint8_t ch);
    void sendCharWithStuffing(uint8_t ch);
    void sendEscaped(uint8_t ch);
    void clear();
    void putCharToFrame(uint8_t ch);

    // Buffer allocation
    typedef enum {
        BUFFER_ALLOC_OK,
        BUFFER_ALLOC_NO_MEM,
        BUFFER_ALLOC_ABOVE_MAX
    } BufferAllocRetc;
    BufferAllocRetc checkRxBufferAllocation(uint32_t maxWriteIdx);
};
