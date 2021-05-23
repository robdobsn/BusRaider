/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RdWebServerSettings.h"
#include "RdWebConnManager.h"
#include "RdWebHandler.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

class RdWebServer
{
public:

    // Constructor
    RdWebServer();

    // Setup the web server and start listening
    void setup(RdWebServerSettings& settings); 

    // Service
    void service();
    
    // Configure
    void addResponseHeader(RdJson::NameValuePair headerInfo);
    
    // Handler
    bool addHandler(RdWebHandler* pHandler);

    // Get max websockets
    uint32_t getMaxWebSockets()
    {
        return _webServerSettings._maxWebSockets;
    }

    // Check if websocket can send
    bool webSocketCanSend(uint32_t protocolChannelID);

    // Send on a websocket (or all websockets)
    bool webSocketSendMsg(const uint8_t* pBuf, uint32_t bufLen, 
                bool allWebSockets, uint32_t protocolChannelID);

    // Send to all server-side events
    void serverSideEventsSendMsg(const char* eventContent, const char* eventGroup);

private:

    static const uint32_t WEB_SERVER_SOCKET_RETRY_DELAY_MS = 1000;
#ifndef ESP8266
    // Helpers
    static void socketListenerTask(void* pvParameters);
#else
    WiFiServer* _pWiFiServer;
#endif
    // Settings
    RdWebServerSettings _webServerSettings;

    // Connection manager
    RdWebConnManager _connManager;

};

