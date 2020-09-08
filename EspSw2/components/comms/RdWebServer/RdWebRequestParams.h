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
    RdWebRequestParams(uint32_t maxSendSize, std::list<RdJson::NameValuePair>* pResponseHeaders,
                    RdWebConnSendFn webConnRawSend)
    {
        _maxSendSize = maxSendSize;
        _pResponseHeaders = pResponseHeaders;
        _webConnRawSend = webConnRawSend;
    }
    uint32_t getMaxSendSize()
    {
        return _maxSendSize;
    }
    RdWebConnSendFn getWebConnRawSend() const
    {
        return _webConnRawSend;
    }
    
private:
    uint32_t _maxSendSize;
    std::list<RdJson::NameValuePair>* _pResponseHeaders;
    RdWebConnSendFn _webConnRawSend;
};
