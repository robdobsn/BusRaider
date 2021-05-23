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
#include <RdWebRequestParams.h>
#include <RdWebConnection.h>
#include <RdWebSocketLink.h>
#include <RdWebSSEvent.h>
#include <Logger.h>
#include <ThreadSafeQueue.h>

class RdWebHandler;
class RdWebServerSettings;
class ProtocolEndpointManager;

// Callback function for server-side-events
typedef std::function<void(const uint8_t* pBuf, uint32_t bufLen)> RdWebSSEventsCB;

class RdWebResponderSSEvents : public RdWebResponder
{
public:
    RdWebResponderSSEvents(RdWebHandler* pWebHandler, const RdWebRequestParams& params, 
                const String& reqStr, RdWebSSEventsCB eventsCallback, 
                const RdWebServerSettings& webServerSettings);
    virtual ~RdWebResponderSSEvents();

    // Service - called frequently
    virtual void service() override final;

    // Handle inbound data
    virtual bool handleData(const uint8_t* pBuf, uint32_t dataLen) override final;

    // Start responding
    virtual bool startResponding(RdWebConnection& request) override final;

    // Get response next
    virtual uint32_t getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen) override final;

    // Get content type
    virtual const char* getContentType() override final;

    // Leave connection open
    virtual bool leaveConnOpen() override final;

    // Send standard headers
    virtual bool isStdHeaderRequired() override final
    {
        return false;
    }

    // Send event content and group
    virtual void sendEvent(const char* eventContent, const char* eventGroup);

    // Get responder type
    virtual const char* getResponderType() override final
    {
        return "SSEvents";
    }

private:
    // Handler
    RdWebHandler* _pWebHandler;

    // Params
    RdWebRequestParams _reqParams;

    // Events callback
    RdWebSSEventsCB _eventsCB;

    // Vars
    String _requestStr;
    bool _isInitialResponse;

    // Queue for sending frames over the event channel
    static const uint32_t EVENT_TX_QUEUE_SIZE = 2;
    ThreadSafeQueue<RdWebSSEvent> _txQueue;

    // Generate event message
    String generateEventMessage(const char *pMsg, const char *pEvent, uint32_t id);
};
