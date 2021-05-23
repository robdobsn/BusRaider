/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebResponderSSEvents.h"
#include <RdWebConnection.h>
#include "RdWebServerSettings.h"
// #include "ProtocolEndpointManager.h"
#include <Logger.h>
#include <Utils.h>

// #define DEBUG_RESPONDER_EVENTS
#define WARN_EVENTS_SEND_APP_DATA_FAIL

#if defined(DEBUG_RESPONDER_EVENTS) || defined(WARN_EVENTS_SEND_APP_DATA_FAIL)
static const char *MODULE_PREFIX = "RdWebRespSSEvents";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponderSSEvents::RdWebResponderSSEvents(RdWebHandler *pWebHandler, const RdWebRequestParams &params,
                                               const String &reqStr, RdWebSSEventsCB eventsCallback,
                                               const RdWebServerSettings &webServerSettings)
    : _reqParams(params), _eventsCB(eventsCallback)
{
    // Store socket info
    _pWebHandler = pWebHandler;
    _requestStr = reqStr;
    _isInitialResponse = true;
    _txQueue.setMaxLen(EVENT_TX_QUEUE_SIZE);
}

RdWebResponderSSEvents::~RdWebResponderSSEvents()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderSSEvents::service()
{
    // Check for data waiting to be sent
    RdWebSSEvent event;
    if (_txQueue.get(event))
    {
#ifdef DEBUG_RESPONDER_EVENTS
        LOG_W(MODULE_PREFIX, "service sendMsg group %s content %s", event.getGroup().c_str(), event.getContent().c_str());
#endif
        // Format message
        String outMsg = generateEventMessage(event.getContent().c_str(), event.getGroup().c_str(), time(NULL));
        if (_reqParams.getWebConnRawSend())
        {
            bool rslt = _reqParams.getWebConnRawSend()((const uint8_t*)outMsg.c_str(), outMsg.length());
            if (!rslt)
                _isActive = false;
        }
        // _webSocketLink.sendMsg(WEBSOCKET_OPCODE_BINARY, frame.getData(), frame.getLen());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle inbound data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderSSEvents::handleData(const uint8_t *pBuf, uint32_t dataLen)
{
#ifdef DEBUG_RESPONDER_EVENTS
    String outStr;
    Utils::strFromBuffer(pBuf, dataLen, outStr, false);
    LOG_I(MODULE_PREFIX, "handleData len %d %s", dataLen, outStr.c_str());
#endif

    // // Handle it with link
    // _webSocketLink.handleRxData(pBuf, dataLen);

    // // Check if the link is still active
    // if (!_webSocketLink.isActive())
    // {
    //     _isActive = false;
    // }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start responding
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderSSEvents::startResponding(RdWebConnection &request)
{
    // // Set link to upgrade-request already received state
    // _webSocketLink.upgradeReceived(request.getHeader().webSocketKey,
    //                     request.getHeader().webSocketVersion);

    // Now active
    _isActive = true;
#ifdef DEBUG_RESPONDER_EVENTS
    LOG_I(MODULE_PREFIX, "startResponding isActive %d", _isActive);
#endif
    return _isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get response next
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebResponderSSEvents::getResponseNext(uint8_t *pBuf, uint32_t bufMaxLen)
{
    uint32_t respLen = 0;

    // Check if initial response
    if (_isInitialResponse)
    {
        // Response for snprintf
        const char SSEVENT_RESPONSE[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Accept-Ranges: none\r\n\r\n";

        // Reponse string
        snprintf((char *)pBuf, bufMaxLen, SSEVENT_RESPONSE);

        // Response done
        _isInitialResponse = false;

        // Debug
#ifdef DEBUG_RESPONDER_EVENTS
        LOG_I(MODULE_PREFIX, "getResponseNext isActive %d resp %s", _isActive, (char *)pBuf);
#endif

        // Return
        return strlen((char *)pBuf);
    }

    // // Get
    // uint32_t respLen = _webSocketLink.getTxData(pBuf, bufMaxLen);

    // Done response
#ifdef DEBUG_RESPONDER_EVENTS
    if (respLen > 0)
    {
        LOG_I(MODULE_PREFIX, "getResponseNext respLen %d isActive %d", respLen, _isActive);
    }
#endif
    return respLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char *RdWebResponderSSEvents::getContentType()
{
    return "application/octet-stream";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leave connection open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderSSEvents::leaveConnOpen()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send event content and group
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderSSEvents::sendEvent(const char* eventContent, const char* eventGroup)
{
    // Add to queue - don't block if full
    RdWebSSEvent event(eventContent, eventGroup);
    bool putRslt = _txQueue.put(event);
    if (!putRslt)
    {
#ifdef WARN_EVENTS_SEND_APP_DATA_FAIL
        LOG_W(MODULE_PREFIX, "sendFrame failed group %s content %s", eventGroup, eventContent);
#endif
    }
    else
    {
#ifdef DEBUG_RESPONDER_EVENTS
        LOG_W(MODULE_PREFIX, "sendFrame ok group %s content %s", eventGroup, eventContent);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send a frame of data
// From ESPAsyncWebServer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdWebResponderSSEvents::generateEventMessage(const char *pMsg, const char *pEvent, uint32_t id)
{
    String ev = "";

    if (id)
    {
        ev += "id: ";
        ev += String(id);
        ev += "\r\n";
    }

    if (pEvent != NULL)
    {
        ev += "event: ";
        ev += String(pEvent);
        ev += "\r\n";
    }

    if (pMsg != NULL)
    {
        size_t messageLen = strlen(pMsg);
        char *lineStart = (char *)pMsg;
        char *lineEnd;
        do
        {
            char *nextN = strchr(lineStart, '\n');
            char *nextR = strchr(lineStart, '\r');
            if (nextN == NULL && nextR == NULL)
            {
                size_t llen = ((char*)pMsg + messageLen) - lineStart;
                char *ldata = (char*)malloc(llen + 1);
                if (ldata != NULL)
                {
                    memcpy(ldata, lineStart, llen);
                    ldata[llen] = 0;
                    ev += "data: ";
                    ev += ldata;
                    ev += "\r\n\r\n";
                    free(ldata);
                }
                lineStart = (char*)pMsg + messageLen;
            }
            else
            {
                char *nextLine = NULL;
                if (nextN != NULL && nextR != NULL)
                {
                    if (nextR < nextN)
                    {
                        lineEnd = nextR;
                        if (nextN == (nextR + 1))
                            nextLine = nextN + 1;
                        else
                            nextLine = nextR + 1;
                    }
                    else
                    {
                        lineEnd = nextN;
                        if (nextR == (nextN + 1))
                            nextLine = nextR + 1;
                        else
                            nextLine = nextN + 1;
                    }
                }
                else if (nextN != NULL)
                {
                    lineEnd = nextN;
                    nextLine = nextN + 1;
                }
                else
                {
                    lineEnd = nextR;
                    nextLine = nextR + 1;
                }

                size_t llen = lineEnd - lineStart;
                char *ldata = (char *)malloc(llen + 1);
                if (ldata != NULL)
                {
                    memcpy(ldata, lineStart, llen);
                    ldata[llen] = 0;
                    ev += "data: ";
                    ev += ldata;
                    ev += "\r\n";
                    free(ldata);
                }
                lineStart = nextLine;
                if (lineStart == ((char*)pMsg + messageLen))
                    ev += "\r\n";
            }
        } while (lineStart < ((char*)pMsg + messageLen));
    }

    return ev;
}
