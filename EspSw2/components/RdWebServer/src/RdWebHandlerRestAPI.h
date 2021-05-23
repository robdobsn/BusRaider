/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include "RdWebHandler.h"
#include "RdWebInterface.h"
#include "RdWebRequestHeader.h"
#include "RdWebResponderRestAPI.h"

// #define DEBUG_WEB_HANDLER_REST_API

class RdWebRequest;

class RdWebHandlerRestAPI : public RdWebHandler
{
public:
    RdWebHandlerRestAPI(const String& restAPIPrefix, RdWebAPIMatchEndpointCB matchEndpointCB)
    {
        _matchEndpointCB = matchEndpointCB;
        _restAPIPrefix = restAPIPrefix;
        if (!_restAPIPrefix.startsWith("/"))
            _restAPIPrefix = "/" + _restAPIPrefix;
    }
    virtual ~RdWebHandlerRestAPI()
    {
    }
    virtual const char* getName() override
    {
        return "HandlerRESTAPI";
    }
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
            const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings) override final
    {
        // Check
        if (!_matchEndpointCB)
            return nullptr;

        // Check for API prefix
        if (!requestHeader.URL.startsWith(_restAPIPrefix))
        {
#ifdef DEBUG_WEB_HANDLER_REST_API
            LOG_W("WebHandlerRestAPI", "getNewResponder no match with %s for %s", 
                        _restAPIPrefix.c_str(), requestHeader.URL.c_str());
#endif
            return nullptr;
        }

        // Remove prefix on test string
        String reqStr = requestHeader.URIAndParams.substring(_restAPIPrefix.length());
        RdWebServerRestEndpoint endpoint;
        if (!_matchEndpointCB(reqStr.c_str(), requestHeader.extract.method, endpoint))
        {
#ifdef DEBUG_WEB_HANDLER_REST_API
            LOG_W("WebHandlerRestAPI", "getNewResponder no matching endpoint found %s", requestHeader.URL.c_str());
#endif
            return nullptr;
        }
        // Looks like we can handle this so create a new responder object
        RdWebResponder* pResponder = new RdWebResponderRestAPI(endpoint, this, params, reqStr, requestHeader.extract);

        // Debug
#ifdef DEBUG_WEB_HANDLER_REST_API
        LOG_I("WebHandlerRestAPI", "getNewResponder constructed new responder %lx uri %s", (unsigned long)pResponder, requestHeader.URL.c_str());
#endif

        // Return new responder - caller must clean up by deleting object when no longer needed
        return pResponder;
    }

private:
    RdWebAPIMatchEndpointCB _matchEndpointCB;
    String _restAPIPrefix;
};
