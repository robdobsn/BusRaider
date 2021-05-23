/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266

#include "RdWebResponderWS.h"
#include <RdWebConnection.h>
#include "RdWebServerSettings.h"
#include "RdWebInterface.h"
#include <Logger.h>
#include <Utils.h>

// #define DEBUG_RESPONDER_WS
// #define DEBUG_WS_SEND_APP_DATA
// #define DEBUG_WS_SEND_APP_DATA_ASCII
// #define DEBUG_WEBSOCKETS_OPEN_CLOSE
// #define DEBUG_WEBSOCKETS_TRAFFIC
// #define DEBUG_WEBSOCKETS_PING_PONG
#define WARN_WS_SEND_APP_DATA_FAIL

#if defined(WARN_WS_SEND_APP_DATA_FAIL)
static const char *MODULE_PREFIX = "RdWebRespWS";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponderWS::RdWebResponderWS(RdWebHandler* pWebHandler, const RdWebRequestParams& params, 
            const String& reqStr, const RdWebServerSettings& webServerSettings,
            RdWebSocketCanAcceptCB canAcceptMsgCB, RdWebSocketMsgCB sendMsgCB)
    : _reqParams(params), _canAcceptMsgCB(canAcceptMsgCB), _sendMsgCB(sendMsgCB)
{
    // Store socket info
    _pWebHandler = pWebHandler;
    _requestStr = reqStr;
    _txQueue.setMaxLen(WEB_SOCKET_TX_QUEUE_SIZE);

    // Init socket link
    _webSocketLink.setup(std::bind(&RdWebResponderWS::webSocketCallback, this, 
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                params.getWebConnRawSend(), webServerSettings._pingIntervalMs, true);
}

RdWebResponderWS::~RdWebResponderWS()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderWS::service()
{
    // Service the link
    _webSocketLink.service();

    // Check for data waiting to be sent
    RdWebDataFrame frame;
    if (_txQueue.get(frame))
    {
#ifdef DEBUG_WS_SEND_APP_DATA
        LOG_W(MODULE_PREFIX, "service sendMsg len %d", frame.getLen());
#endif
        // Send
        _webSocketLink.sendMsg(WEBSOCKET_OPCODE_BINARY, frame.getData(), frame.getLen());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle inbound data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::handleData(const uint8_t* pBuf, uint32_t dataLen)
{
#ifdef DEBUG_RESPONDER_WS
#ifdef DEBUG_WS_SEND_APP_DATA_ASCII
    String outStr;
    Utils::strFromBuffer(pBuf, dataLen, outStr, false);
    LOG_I(MODULE_PREFIX, "handleData len %d %s", dataLen, outStr.c_str());
#else
    LOG_I(MODULE_PREFIX, "handleData len %d", dataLen);
#endif
#endif

    // Handle it with link
    _webSocketLink.handleRxData(pBuf, dataLen);

    // Check if the link is still active
    if (!_webSocketLink.isActive())
    {
        _isActive = false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready for data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::readyForData()
{
    if (_canAcceptMsgCB)
        return _canAcceptMsgCB(_reqParams.getProtocolChannelID());
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start responding
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::startResponding(RdWebConnection& request)
{
    // Set link to upgrade-request already received state
    _webSocketLink.upgradeReceived(request.getHeader().webSocketKey, 
                        request.getHeader().webSocketVersion);

    // Now active
    _isActive = true;
#ifdef DEBUG_RESPONDER_WS
    LOG_I(MODULE_PREFIX, "startResponding isActive %d", _isActive);
#endif
    return _isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get response next
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebResponderWS::getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen)
{
    // Get from the WSLink
    uint32_t respLen = _webSocketLink.getTxData(pBuf, bufMaxLen);

    // Done response
#ifdef DEBUG_RESPONDER_WS
    if (respLen > 0) {
        LOG_I(MODULE_PREFIX, "getResponseNext respLen %d isActive %d", respLen, _isActive);
    }
#endif
    return respLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* RdWebResponderWS::getContentType()
{
    return "application/octet-stream";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leave connection open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::leaveConnOpen()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Send a frame of data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::sendFrame(const uint8_t* pBuf, uint32_t bufLen)
{
    // Add to queue - don't block if full
    RdWebDataFrame frame(pBuf, bufLen);
    bool putRslt = _txQueue.put(frame);
    if (!putRslt)
    {
#ifdef WARN_WS_SEND_APP_DATA_FAIL
        LOG_W(MODULE_PREFIX, "sendFrame failed len %d", bufLen);
#endif
    }
    else
    {
#ifdef DEBUG_WS_SEND_APP_DATA
        LOG_W(MODULE_PREFIX, "sendFrame len %d", bufLen);
#endif
    }
    return putRslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Websocket callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderWS::webSocketCallback(RdWebSocketEventCode eventCode, const uint8_t* pBuf, uint32_t bufLen)
{
#ifdef DEBUG_WEBSOCKETS
	const static char* MODULE_PREFIX = "wsCB";
#endif
	switch(eventCode) 
    {
		case WEBSOCKET_EVENT_CONNECT:
        {
#ifdef DEBUG_WEBSOCKETS_OPEN_CLOSE
			LOG_I(MODULE_PREFIX, "connected!");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_EXTERNAL:
        {
#ifdef DEBUG_WEBSOCKETS_OPEN_CLOSE
			LOG_I(MODULE_PREFIX, "sent a disconnect message");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_INTERNAL:
        {
#ifdef DEBUG_WEBSOCKETS_OPEN_CLOSE
			LOG_I(MODULE_PREFIX, "was disconnected");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_ERROR:
        {
#ifdef DEBUG_WEBSOCKETS_OPEN_CLOSE
			LOG_I(MODULE_PREFIX, "was disconnected due to an error");
#endif
			break;
        }
		case WEBSOCKET_EVENT_TEXT:
        {
            // Send the message
            if (_sendMsgCB && (pBuf != NULL))
                _sendMsgCB(_reqParams.getProtocolChannelID(), (uint8_t*) pBuf, bufLen);
#ifdef DEBUG_WEBSOCKETS_TRAFFIC
            String msgText;
            if (pBuf)
                Utils::strFromBuffer(pBuf, bufLen, msgText);
			LOG_I(MODULE_PREFIX, "sent text message of size %i content %s", bufLen, msgText.c_str());
#endif
			break;
        }
		case WEBSOCKET_EVENT_BINARY:
        {
            // Send the message
            if (_sendMsgCB && (pBuf != NULL))
                _sendMsgCB(_reqParams.getProtocolChannelID(), (uint8_t*) pBuf, bufLen);
#ifdef DEBUG_WEBSOCKETS_TRAFFIC
			LOG_I(MODULE_PREFIX, "sent binary message of size %i", bufLen);
#endif
			break;
        }
		case WEBSOCKET_EVENT_PING:
        {
#ifdef DEBUG_WEBSOCKETS_PING_PONG
			LOG_I(MODULE_PREFIX, "pinged us with message of size %i", bufLen);
#endif
			break;
        }
		case WEBSOCKET_EVENT_PONG:
        {
#ifdef DEBUG_WEBSOCKETS_PING_PONG
			LOG_I(MODULE_PREFIX, "responded to the ping");
#endif
		    break;
        }
        default:
        {
            break;
        }
	}
}

#endif
