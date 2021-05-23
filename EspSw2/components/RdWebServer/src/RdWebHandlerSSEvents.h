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
#include <functional>
#include <RdWebResponderSSEvents.h>

class RdWebRequest;

class RdWebHandlerSSEvents : public RdWebHandler
{
public:
    RdWebHandlerSSEvents(const String& eventsPath, RdWebSSEventsCB eventCallback)
            : _eventCallback(eventCallback)
    {
        _eventsPath = eventsPath;
    }
    virtual ~RdWebHandlerSSEvents()
    {
    }
    virtual const char* getName() override
    {
        return "HandlerSSEvents";
    }
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
                const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings) override final
    {
        // LOG_W("RdWebHandlerSSEvents", "getNewResponder %s connType %d method %d", 
        //             requestHeader.URL, requestHeader.reqConnType, requestHeader.extract.method);

        // Check if event-stream
        if (requestHeader.reqConnType != REQ_CONN_TYPE_EVENT)
            return NULL;

        // Check for correct prefix
        if (!requestHeader.URL.startsWith(_eventsPath))
        {
            LOG_W("WebHandlerSSEvents", "getNewResponder unmatched url req %s != expected %s", requestHeader.URL.c_str(), _eventsPath.c_str());
            return NULL;
        }

        // We can handle this so create a new responder object
        RdWebResponder* pResponder = new RdWebResponderSSEvents(this, params, requestHeader.URL, 
                    _eventCallback, webServerSettings);

        // Debug
        // LOG_W("WebHandlerSSEvents", "getNewResponder constructed new responder %lx uri %s", (unsigned long)pResponder, requestHeader.URL.c_str());

        // Return new responder - caller must clean up by deleting object when no longer needed
        return pResponder;
    }

private:
    String _eventsPath;
    RdWebSSEventsCB _eventCallback;
};
