/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <RdJson.h>
#include <list>
#include "RdWebConnDefs.h"

class RdWebRequestParams
{
public:
    RdWebRequestParams(uint32_t maxSendSize, 
            std::list<RdJson::NameValuePair>* pResponseHeaders,
            RdWebConnSendFn webConnRawSend)
    {
        _maxSendSize = maxSendSize;
        _pResponseHeaders = pResponseHeaders;
        _webConnRawSend = webConnRawSend;
        _protocolChannelID = 0;
    }
    uint32_t getMaxSendSize()
    {
        return _maxSendSize;
    }
    RdWebConnSendFn getWebConnRawSend() const
    {
        return _webConnRawSend;
    }
    void setProtocolChannelID(uint32_t protocolChannelID)
    {
        _protocolChannelID = protocolChannelID;
    }
    uint32_t getProtocolChannelID() const
    {
        return _protocolChannelID;
    }
    
private:
    uint32_t _maxSendSize;
    std::list<RdJson::NameValuePair>* _pResponseHeaders;
    RdWebConnSendFn _webConnRawSend;
    uint32_t _protocolChannelID;
};
