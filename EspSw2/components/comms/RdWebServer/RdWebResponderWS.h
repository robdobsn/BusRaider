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
#include <Logger.h>
#include <RingBufferRTOS.h>

class RdWebHandler;
class RdWebServerSettings;

class RdWebResponderWS : public RdWebResponder
{
public:
    RdWebResponderWS(RdWebHandler* pWebHandler, const RdWebRequestParams& params, 
                const String& reqStr, RdWebSocketCB webSocketCB, 
                const RdWebServerSettings& webServerSettings);
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
    virtual void sendFrame(const uint8_t* pBuf, uint32_t bufLen) override final;

    // Get responder type
    virtual const char* getResponderType() override final
    {
        return "WS";
    }

private:
    // Handler
    RdWebHandler* _pWebHandler;

    // Params
    RdWebRequestParams _reqParams;

    // Websocket callback
    RdWebSocketCB _webSocketCB;

    // Websocket link
    RdWebSocketLink _webSocketLink;

    // Vars
    String _requestStr;

    // Buffer for tx queue
    class RdWebDataFrame
    {
    public:
        RdWebDataFrame()
        {
        }
        RdWebDataFrame(const uint8_t* pBuf, uint32_t bufLen)
        {
            frame.resize(bufLen);
            memcpy(frame.data(), pBuf, frame.size());
        }
        const uint8_t* getData()
        {
            return frame.data();
        }
        uint32_t getLen()
        {
            return frame.size();
        }
    private:
        std::vector<uint8_t> frame;
    };

    // Queue for sending frames over the web socket
    static const uint32_t WEB_SOCKET_TX_QUEUE_SIZE = 3;
    RingBufferRTOS<RdWebDataFrame, WEB_SOCKET_TX_QUEUE_SIZE> _txQueue;
};
