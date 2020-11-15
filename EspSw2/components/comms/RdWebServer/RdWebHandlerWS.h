/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RdWebHandler.h"
#include <Logger.h>
#include <RdWebRequestHeader.h>
#include <RdWebResponderWS.h>

class RdWebRequest;

class RdWebHandlerWS : public RdWebHandler
{
public:
    RdWebHandlerWS(ProtocolEndpointManager* pProtocolEndpointManager, const String& wsPath)
            : _pProtocolEndpointManager(pProtocolEndpointManager)
    {
        _wsPath = wsPath;
    }
    virtual ~RdWebHandlerWS()
    {
    }
    virtual const char* getName() override
    {
        return "HandlerWS";
    }
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
                const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings) override final
    {
        // Check if websocket request
        if (requestHeader.reqConnType != REQ_CONN_TYPE_WEBSOCKET)
            return NULL;

        // Check for WS prefix
        if (!requestHeader.URL.startsWith(_wsPath))
        {
            LOG_W("WebHandlerWS", "getNewResponder unmatched ws req %s != expected %s", requestHeader.URL.c_str(), _wsPath.c_str());
            return NULL;
        }

        // Looks like we can handle this so create a new responder object
        RdWebResponder* pResponder = new RdWebResponderWS(this, params, requestHeader.URL, 
                    _pProtocolEndpointManager, webServerSettings);

        // Debug
        // LOG_W("WebHandlerWS", "getNewResponder constructed new responder %lx uri %s", (unsigned long)pResponder, requestHeader.URL.c_str());

        // Return new responder - caller must clean up by deleting object when no longer needed
        return pResponder;
    }

private:
    String _wsPath;
    ProtocolEndpointManager* _pProtocolEndpointManager;
};
