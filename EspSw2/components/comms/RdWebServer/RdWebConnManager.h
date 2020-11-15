/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RdWebServerSettings.h"
#include <RestAPIEndpointManager.h>
#include <RdWebConnection.h>
#include <RdWebSocketDefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <list>

class RdWebHandler;
class RdWebRequestHeader;
class RdWebResponder;
class RdWebRequestParams;
class ProtocolEndpointManager;
class ProtocolEndpointMsg;

class RdWebConnManager
{
public:
    // Constructor
    RdWebConnManager();

    // Setup
    void setup(RdWebServerSettings& settings);

    // Service
    void service();

    // Handle an incoming connection
    bool handleNewConnection(struct netconn* pNewConnection);

    // Endpoints
    void addRestAPIEndpoints(RestAPIEndpointManager* pEndpoints);
    void addProtocolEndpoints(ProtocolEndpointManager& endpointManager);

    // Handler
    void addHandler(RdWebHandler* pHandler);

    // Add response headers
    void addResponseHeader(RdJson::NameValuePair headerInfo)
    {
        _stdResponseHeaders.push_back(headerInfo);
    }

    // New HTTP connection
    void onConnection(struct netconn *pConnection);

    // Get new responder
    // NOTE: this returns a new object or NULL
    // NOTE: if a new object is returned the caller is responsible for deleting it when appropriate
    RdWebResponder* getNewResponder(const RdWebRequestHeader& header, 
                const RdWebRequestParams& params, HttpStatusCode& statusCode);

    // Get standard response headers
    std::list<RdJson::NameValuePair>* getStdResponseHeaders()
    {
        return &_stdResponseHeaders;
    }

    // Get server settings
    RdWebServerSettings getServerSettings()
    {
        return _webServerSettings;
    }

    // Send to all websockets
    void sendToAllWebSockets(const uint8_t* pBuf, uint32_t bufLen);

private:
    // New connection queue
    QueueHandle_t _newConnQueue;
    static const int _newConnQueueMaxLen = 10;

    // Mutex for handling endpoints
    SemaphoreHandle_t _endpointsMutex;

    // Endpoint manager
    RestAPIEndpointManager* _pEndpointManager;

    // Web server settings
    RdWebServerSettings _webServerSettings;

    // Handlers
    std::list<RdWebHandler*> _webHandlers;

    // Standard response headers
    std::list<RdJson::NameValuePair> _stdResponseHeaders;

    // Connections
    std::vector<RdWebConnection> _webConnections;

    // Web socket protocol channelIDs
    std::vector<uint32_t> _webSocketProtocolChannelIDs;

    // Web socket channel mapping
    std::vector<int> _webSocketChannelMapping;

    // Client connection handler task
    static void clientConnHandlerTask(void* pvParameters);

    // Helpers
    bool accommodateConnection(struct netconn* pNewConnection);
    bool findEmptySlot(uint32_t& slotIx);
    uint32_t countWebSockets();
    void serviceConnections();
    bool allocateWebSocketChannelID(uint32_t& protocolChannelID);
    bool sendWebSocketMsg(ProtocolEndpointMsg& msg);
    bool readyToSendWebSocketMsg();
};

