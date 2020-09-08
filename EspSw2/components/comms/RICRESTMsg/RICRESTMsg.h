/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RICRESTMsg
// Message encapsulation for REST message
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <WString.h>

static const uint32_t RICREST_REST_ELEM_CODE_POS = 0;
static const uint32_t RICREST_HEADER_PAYLOAD_POS = 1;
static const uint32_t RICREST_HEADER_MIN_MSG_LEN = 4;
static const uint32_t RICREST_BODY_BUFFER_POS = 1;
static const uint32_t RICREST_BODY_TOTAL_POS = 5;
static const uint32_t RICREST_BODY_PAYLOAD_POS = 9;
static const uint32_t RICREST_COMMAND_FRAME_PAYLOAD_POS = 1;
static const uint32_t RICREST_FILEBLOCK_FILE_POS = 1;
static const uint32_t RICREST_FILEBLOCK_PAYLOAD_POS = 5;

// TODO - ensure this is long enough for all needs
static const uint32_t RICREST_MAX_PAYLOAD_LEN = 5000;

class ProtocolEndpointMsg;

class RICRESTMsg
{
public:
    enum RICRESTElemCode
    {
        RICREST_REST_ELEM_URL,
        RICREST_REST_ELEM_CMDRESPJSON,
        RICREST_REST_ELEM_BODY,
        RICREST_REST_ELEM_COMMAND_FRAME,
        RICREST_REST_ELEM_FILEBLOCK
    };

    static const uint32_t MAX_REST_BODY_SIZE = 5000;
    RICRESTMsg()
    {
        _RICRESTElemCode = RICREST_REST_ELEM_URL;
        _bufferPos = 0;
        _binaryLen = 0;
        _pBinaryData = NULL;
        _totalBytes = 0;
    }
    bool decode(const uint8_t* pBuf, uint32_t len);
    void encode(const String& payload, ProtocolEndpointMsg& endpointMsg);

    const String& getReq()
    {
        return _req;
    }
    const String& getPayloadJson()
    {
        return _payloadJson;
    }
    const uint8_t* getBinBuf()
    {
        return _pBinaryData;
    }
    uint32_t getBinLen()
    {
        return _binaryLen;
    }
    uint32_t getBufferPos()
    {
        return _bufferPos;
    }
    uint32_t getTotalBytes()
    {
        return _totalBytes;
    }
    RICRESTElemCode getElemCode()
    {
        return _RICRESTElemCode;
    }
    void setElemCode(RICRESTElemCode elemCode)
    {
        _RICRESTElemCode = elemCode;
    }

private:
    // Store Uint32
    uint32_t uint32From(const uint8_t* pBuf, uint32_t& offset, uint32_t maxLen)
    {
        uint32_t val = 0;
        if (offset + 4 < maxLen)
        {
            val = pBuf[offset++];
            val = (val << 8) + pBuf[offset++];
            val = (val << 8) + pBuf[offset++];
            val = (val << 8) + pBuf[offset++];
        }
        else
        {
            offset += 4;
        }
        return val;
    }

    // Get string
    void getStringFromBuf(String& destStr, const uint8_t* pBuf, uint32_t offset, uint32_t len, uint32_t maxLen)
    {    
        // Ensure length is ok
        uint32_t lenToCopy = len-offset+1;
        if (lenToCopy > maxLen)
            lenToCopy = maxLen;
        destStr = "";
        if (lenToCopy < 1)
            return;
        // Set into string without allocating additional buffer space
        destStr.concat((const char*)(pBuf+offset), lenToCopy-1);
    }

    // RICRESTElemCode
    RICRESTElemCode _RICRESTElemCode;

    // Parameters
    String _req;
    String _payloadJson;
    uint32_t _bufferPos;
    uint32_t _binaryLen;
    uint32_t _totalBytes;
    const uint8_t* _pBinaryData;
};
