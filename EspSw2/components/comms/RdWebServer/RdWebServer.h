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

    // Endpoints
    void addRestAPIEndpoints(RestAPIEndpointManager* pEndpoints);
    void addProtocolEndpoints(ProtocolEndpointManager& endpointManager);

    // Handler
    void addHandler(RdWebHandler* pHandler);

    // Send to all WebSockets
    void sendToAllWebSockets(const uint8_t* pBuf, uint32_t bufLen);

private:
    // Helpers
    static void socketListenerTask(void* pvParameters);

    // Settings
    RdWebServerSettings _webServerSettings;

    // Connection manager
    RdWebConnManager _connManager;

};

