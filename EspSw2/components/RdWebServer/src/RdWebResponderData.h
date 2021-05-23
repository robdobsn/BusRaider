/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include "RdWebResponder.h"
#include "RdWebRequestParams.h"

// #define DEBUG_STATIC_DATA_RESPONDER

class RdWebHandler;

class RdWebResponderData : public RdWebResponder
{
public:
    RdWebResponderData( const uint8_t* pData, uint32_t dataLen, const char* pMIMEType,
                RdWebHandler* pWebHandler, const RdWebRequestParams& params)
            : _reqParams(params)
    {
        _pWebHandler = pWebHandler;
        _mimeType = pMIMEType;
        _curDataPos = 0;
        _dataLength = dataLen;
        _pData = pData;
    }

    virtual ~RdWebResponderData()
    {
    }

    // Handle inbound data
    virtual bool handleData(const uint8_t* pBuf, uint32_t dataLen) override final
    {
        return true;
    }

    // Start responding
    virtual bool startResponding(RdWebConnection& request) override final
    {
        _isActive = true;
        _curDataPos = 0;
        return _isActive;
    }

    // Get response next
    virtual uint32_t getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen) override final
    {
        uint32_t lenToCopy = _dataLength - _curDataPos;
        if (lenToCopy > bufMaxLen)
            lenToCopy = bufMaxLen;
        if (!_isActive || (lenToCopy == 0))
        {
#ifdef DEBUG_STATIC_DATA_RESPONDER
            LOG_I("WebRespData", "getResponseNext NOTHING TO RETURN");
#endif
            _isActive = false;
            return 0;
        }
#ifdef DEBUG_STATIC_DATA_RESPONDER
        LOG_I("WebRespData", "getResponseNext pos %d totalLen %d lenToCopy %d isActive %d ptr %x", 
                    _curDataPos, _dataLength, lenToCopy, _isActive, _pData);
#endif
        memcpy_P(pBuf, _pData+_curDataPos, lenToCopy);
        _curDataPos += lenToCopy;
        if (_curDataPos >= _dataLength)
        {
            _isActive = false;
        }
#ifdef DEBUG_STATIC_DATA_RESPONDER
        LOG_I("WebRespData", "getResponseNext returning %d newCurPos %d isActive %d", 
                    lenToCopy, _curDataPos, _isActive);
#endif
        return lenToCopy;
    }

    // Get content type
    virtual const char* getContentType() override final
    {
        return _mimeType.c_str();
    }

    // Get content length (or -1 if not known)
    virtual int getContentLength() override final
    {
        return _dataLength;
    }

    // Leave connection open
    virtual bool leaveConnOpen() override final
    {
        return false;
    }

    // Get responder type
    virtual const char* getResponderType() override final
    {
        return "DATA";
    }

private:
    RdWebHandler* _pWebHandler;
    RdWebRequestParams _reqParams;
    const uint8_t* _pData;
    uint32_t _dataLength;
    uint32_t _curDataPos;
    String _mimeType;
};
