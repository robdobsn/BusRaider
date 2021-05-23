/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ESP8266

#include <WString.h>
#include "RdWebResponder.h"
#include <RdWebRequestParams.h>
#include <RdWebConnection.h>
#include <RdWebSocketLink.h>
#include <RdWebDataFrame.h>
#include <Logger.h>
#include <ThreadSafeQueue.h>
#include "RdWebInterface.h"

class RdWebHandler;
class RdWebServerSettings;
class ProtocolEndpointManager;

class RdWebResponderWS : public RdWebResponder
{
public:
    RdWebResponderWS(RdWebHandler* pWebHandler, const RdWebRequestParams& params,
            const String& reqStr, const RdWebServerSettings& webServerSettings,
            RdWebSocketCanAcceptCB canAcceptMsgCB, RdWebSocketMsgCB sendMsgCB);
    virtual ~RdWebResponderWS();

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

    // Send a frame of data
    virtual bool sendFrame(const uint8_t* pBuf, uint32_t bufLen) override final;

    // Get responder type
    virtual const char* getResponderType() override final
    {
        return "WS";
    }

    // Get protocolChannelID for responder
    virtual bool getProtocolChannelID(uint32_t& protocolChannelID)
    {
        protocolChannelID = _reqParams.getProtocolChannelID();
        return true;
    }

    // Ready for data
    virtual bool readyForData() override final;

private:
    // Handler
    RdWebHandler* _pWebHandler;

    // Params
    RdWebRequestParams _reqParams;

    // Websocket callback
    RdWebSocketCB _webSocketCB;

    // Websocket link
    RdWebSocketLink _webSocketLink;

    // Can accept message function
    RdWebSocketCanAcceptCB _canAcceptMsgCB;

    // Send message function
    RdWebSocketMsgCB _sendMsgCB;

    // Vars
    String _requestStr;

    // Queue for sending frames over the web socket
    static const uint32_t WEB_SOCKET_TX_QUEUE_SIZE = 10;
    ThreadSafeQueue<RdWebDataFrame> _txQueue;

    // Callback on websocket activity
    void webSocketCallback(RdWebSocketEventCode eventCode, const uint8_t* pBuf, uint32_t bufLen);
};

#endif
