/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ESP8266

#include "RdWebHandler.h"
#include <Logger.h>
#include <RdWebRequestHeader.h>
#include <RdWebResponderWS.h>

class RdWebRequest;

class RdWebHandlerWS : public RdWebHandler
{
public:
    RdWebHandlerWS(const String& wsPath, uint32_t maxWebSockets,
            RdWebSocketCanAcceptCB canAcceptRxMsgCB, RdWebSocketMsgCB rxMsgCB)
            : _canAcceptRxMsgCB(canAcceptRxMsgCB), _rxMsgCB(rxMsgCB)
    {
        _wsPath = wsPath;
        _webSocketProtocolChannelIDs.resize(maxWebSockets);
        static const uint32_t WEB_SOCKET_CHANNEL_START_NO = 50;
        uint32_t wsChanId = WEB_SOCKET_CHANNEL_START_NO;
        // Assign incremental channel numbers initially
        for (uint32_t& chanId : _webSocketProtocolChannelIDs)
            chanId = wsChanId++;
    }
    virtual ~RdWebHandlerWS()
    {
    }
    virtual bool isWebSocketHandler() override final
    {
        return true;
    }
    virtual const char* getName() override
    {
        return "HandlerWS";
    }
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
                const RdWebRequestParams& params, 
                const RdWebServerSettings& webServerSettings) override final
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
                    webServerSettings, _canAcceptRxMsgCB, _rxMsgCB);

        // Debug
        // LOG_W("WebHandlerWS", "getNewResponder constructed new responder %lx uri %s", (unsigned long)pResponder, requestHeader.URL.c_str());

        // Return new responder - caller must clean up by deleting object when no longer needed
        return pResponder;
    }

    uint32_t getMaxWebSockets()
    {
        return _webSocketProtocolChannelIDs.size();
    }

    // Define websocket channel ID
    void defineWebSocketChannelID(uint32_t wsIdx, uint32_t chanID)
    {
        // Check valid
        if (wsIdx >= _webSocketProtocolChannelIDs.size())
            return;
        _webSocketProtocolChannelIDs[wsIdx] = chanID;
    }

    virtual void getChannelIDList(std::list<uint32_t>& chanIdList) override final
    {
        chanIdList.clear();
        for (uint32_t chId : _webSocketProtocolChannelIDs)
            chanIdList.push_back(chId);
    }

private:
    // Websocket path
    String _wsPath;

    // WS interface functions
    RdWebSocketCanAcceptCB _canAcceptRxMsgCB;
    RdWebSocketMsgCB _rxMsgCB;

    // Web socket protocol channelIDs
    std::vector<uint32_t> _webSocketProtocolChannelIDs;
};

#endif