/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ESP8266

#include <stdint.h>
#include <vector>
#include <WString.h>
#include "RdWebSocketDefs.h"
#include "RdWebConnDefs.h"

class RdWebSocketLink
{
public:

    // Constructor
    RdWebSocketLink();
    virtual ~RdWebSocketLink();

    // Setup the web socket
    void setup(RdWebSocketCB webSocketCB, RdWebConnSendFn rawConnSendFn, uint32_t pingIntervalMs, bool roleIsServer);

    // Service - called frequently
    void service();

    // Upgrade the link
    void upgradeReceived(const String& wsKey, const String& wsVersion);

    // Handle incoming data
    void handleRxData(const uint8_t* pBuf, uint32_t bufLen);
    
    // Get data to tx
    uint32_t getTxData(uint8_t* pBuf, uint32_t bufMaxLen);

    // Form response to upgrade connection
    uint32_t formUpgradeResponse(const String& wsKey, const String& wsVersion, uint8_t* pBuf, uint32_t bufMaxLen);

    // Gen the hash required for response
    String genMagicResponse(const String& wsKey, const String& wsVersion);

    // Send message
    bool sendMsg(WebSocketOpCodes opCode, const uint8_t* pBuf, uint32_t bufLen);

    // Check active
    bool isActive()
    {
        return _isActive;
    }

    // Helper
    static const char* getEventStr(RdWebSocketEventCode eventCode)
    {
        switch(eventCode)
        {
            case WEBSOCKET_EVENT_CONNECT: return "CONNECT";
            case WEBSOCKET_EVENT_DISCONNECT_EXTERNAL: return "DISCEXT";
            case WEBSOCKET_EVENT_DISCONNECT_INTERNAL: return "DISCINT";
            case WEBSOCKET_EVENT_DISCONNECT_ERROR: return "ERROR";
            case WEBSOCKET_EVENT_TEXT: return "TEXT";
            case WEBSOCKET_EVENT_BINARY: return "BINARY";
            case WEBSOCKET_EVENT_PING: return "PING";
            case WEBSOCKET_EVENT_PONG: return "PONG";
            case WEBSOCKET_EVENT_NONE: return "NONE";
        }
        return "NONE";
    }

private:
    // State
    bool _upgradeReqReceived;
    bool _upgradeRespSent;

    // WS Upgrade info
    String _wsKey;
    String _wsVersion;

    // Data to be sent to callback when complete
    std::vector<uint8_t> _callbackData;
    RdWebSocketCB _webSocketCB;

    // Received data not yet processed
    std::vector<uint8_t> _rxDataToProcess;

    // Raw send on the connection
    RdWebConnSendFn _rawConnSendFn;

    // Active
    bool _isActive;

    // Mask sent data
    bool _maskSentData;

    // Max message size
    static const uint32_t MAX_WS_MESSAGE_SIZE = 5000;

    // Ping/Pong sending
    // Set _pingIntervalMs to 0 to disable pings from server
    uint32_t _pingIntervalMs;
    uint32_t _pingTimeLastMs;
    
    // WS Header Info
    class WSHeaderInfo
    {
    public:
        WSHeaderInfo()
        {
            fin = false;
            mask = 0;
            opcode = 0;
            len = 0;
            maskKey[0] = 0;
            maskKey[1] = 0;
            maskKey[2] = 0;
            maskKey[3] = 0;
            dataPos = 0;
            ignoreUntilFinal = false;
            firstFrameOpcode = 0;
        }
        // Returns length of block (0 indicates header not long enough)
        uint32_t extract(const uint8_t* pBuf, uint32_t bufLen)
        {
            // First two bytes
            uint32_t pos = 0;
            if (bufLen < pos + 2)
                return 0;
            fin = (pBuf[pos] & 0x80) != 0;
            opcode = pBuf[pos] & 0x0f;
            pos += 1;
            mask = (pBuf[pos] & 0x80) != 0;
            len = pBuf[pos] & 0x7f;
            pos += 1;

            // Check if len is complete
            if (len == 126)
            {
                if (bufLen < pos + 2)
                    return 0;
                len = pBuf[pos] * 256 + pBuf[pos+1];
                pos += 2;
            }
            else if (len == 127)
            {
                if (bufLen < pos + 8)
                    return 0;
                len = pBuf[pos++] & 0x7f;
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
                len = (len << 8) + pBuf[pos++];
            }

            // Check for mask
            if (mask)
            {
                if (bufLen < pos + 2)
                    return 0;
                maskKey[0] = pBuf[pos++];
                maskKey[1] = pBuf[pos++];
                maskKey[2] = pBuf[pos++];
                maskKey[3] = pBuf[pos++];
            }

            // Data pos
            dataPos = pos;

            // Check if we should update first-frame opcode
            if (opcode != WEBSOCKET_OPCODE_CONTINUE)
                firstFrameOpcode = opcode;

            // Check length
            return len;
        }

        // Header
        bool fin;
        bool mask;
        uint32_t opcode;
        uint64_t len;
        static const uint32_t WEB_SOCKET_MASK_KEY_BYTES = 4;
        uint8_t maskKey[WEB_SOCKET_MASK_KEY_BYTES];
        uint32_t dataPos;

        // Receive state
        bool ignoreUntilFinal;
        uint32_t firstFrameOpcode;
    };
    WSHeaderInfo _wsHeader;

    // Helpers
    uint32_t handleRxPacketData(const uint8_t* pBuf, uint32_t bufLen);
    uint32_t extractWSHeaderInfo(const uint8_t* pBuf, uint32_t bufLen);
    void unmaskData();

};

#endif
