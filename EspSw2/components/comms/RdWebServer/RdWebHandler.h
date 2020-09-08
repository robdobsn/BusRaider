/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>

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

private:
};

