/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TCPConnection
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "TCPConnSettings.h"
#include <RestAPIEndpointManager.h>
#include <TCPConnection.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <list>
#include <vector>

class TCPConnManager
{
public:
    // Constructor
    TCPConnManager();

    // Setup
    void setup(TCPConnSettings* pSettings, unsigned numSettings);

    // Handle an incoming connection
    bool handleNewConnection(struct netconn* pNewConnection);

    // Endpoints
    void addEndpoints(RestAPIEndpointManager* pEndpoints);

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
    RdWebResponder* getNewResponder(const RdWebRequestHeader& header, const RdWebRequestParams& params);

    // Get server settings
    std::vector<TCPConnSettings>& getServerSettings()
    {
        return _tcpConnSettings;
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

    // TCP settings
    std::vector<TCPConnSettings> _tcpConnSettings;

    // Connections
    std::vector<TCPConnection> _tcpConnections;

    // Client connection handler task
    static void clientConnHandlerTask(void* pvParameters);

    // Helpers
    bool accommodateConnection(struct netconn* pNewConnection);
    bool findEmptySlot(uint32_t& slotIx);
    uint32_t countWebSockets();
};

