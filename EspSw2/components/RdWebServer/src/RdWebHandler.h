/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <list>

class RdWebRequest;
class RdWebRequestParams;
class RdWebRequestHeader;
class RdWebResponder;
class RdWebServerSettings;

class RdWebHandler
{
public:
    RdWebHandler()
    {
    }
    virtual ~RdWebHandler()
    {        
    }
    virtual const char* getName()
    {
        return "HandlerBase";
    }
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
                const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings)
    {
        return NULL;
    }
    virtual bool isFileHandler()
    {
        return false;
    }
    virtual bool isWebSocketHandler()
    {
        return false;
    }
    
    virtual void getChannelIDList(std::list<uint32_t>& chanIdList)
    {
    }

private:
};

