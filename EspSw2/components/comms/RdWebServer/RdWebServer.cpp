/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "RdWebServer.h"
#include "RdWebConnManager.h"
#include "RdWebConnDefs.h"
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include <Utils.h>
#include <NetworkSystem.h>

static const char *MODULE_PREFIX = "RdWebServer";

#define INFO_WEB_SERVER_SETUP
// #define DEBUG_NEW_CONNECTION

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebServer::RdWebServer()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup Web
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::setup(RdWebServerSettings& settings)
{
	// Settings
    _webServerSettings = settings;
    
#ifdef INFO_WEB_SERVER_SETUP
    LOG_I(MODULE_PREFIX, "setup port %d numConnSlots %d enableFileServer %d", 
            settings._serverTCPPort, settings._numConnSlots, settings._enableFileServer);
#endif

    // Start network interface if not already started
#ifdef USE_IDF_V4_1_NETIF_METHODS
        if (esp_netif_init() != ESP_OK)
            LOG_E(MODULE_PREFIX, "could not start netif");
#else
        tcpip_adapter_init();
#endif

	// Setup connection manager
	_connManager.setup(_webServerSettings);

	// Start task to handle listen for connections
	xTaskCreatePinnedToCore(&socketListenerTask,"socketLstnTask", 
            settings._taskStackSize,
            this, 
            settings._taskPriority, 
            NULL, 
            settings._taskCore);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::service()
{
    // Service connection manager
    _connManager.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::addHandler(RdWebHandler* pHandler)
{
    _connManager.addHandler(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Web Server Task
// Listen for connections and add to queue for handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::socketListenerTask(void* pvParameters) 
{
	// Get pointer to specific RdWebServer object
	RdWebServer* pWS = (RdWebServer*)pvParameters;

    // Loop forever
    while (1)
    {
        // Create netconn and bind to port
        struct netconn* pListener = netconn_new(NETCONN_TCP);
        netconn_bind(pListener, NULL, pWS->_webServerSettings._serverTCPPort);
        netconn_listen(pListener);
        LOG_I(MODULE_PREFIX, "web server listening");

        // Wait for connection
        while (true) 
        {
            // Accept connection
            struct netconn* pNewConnection;
            err_t errCode = netconn_accept(pListener, &pNewConnection);
#ifdef DEBUG_NEW_CONNECTION
            LOG_I(MODULE_PREFIX, "new connection netconn %s", netconnErrToStr(errCode));
#endif
            if(errCode == ERR_OK) 
            {
                // Handle the connection
                if (!pWS->_connManager.handleNewConnection(pNewConnection))
                {
                    // No room so close and delete
                    netconn_close(pNewConnection);
                    netconn_delete(pNewConnection);
                }
            }
            else
            {
                break;
            }
        }

        // Listener exited
        netconn_close(pListener);
        netconn_delete(pListener);
        LOG_E(MODULE_PREFIX,"listener exited");

        // Delay hoping networking recovers
        delay(5000);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::addResponseHeader(RdJson::NameValuePair headerInfo)
{
    _connManager.addResponseHeader(headerInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::addRestAPIEndpoints(RestAPIEndpointManager* pEndpointManager)
{
	// Add to handler
	_connManager.addRestAPIEndpoints(pEndpointManager);
}

void RdWebServer::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
	// Add to handler
	_connManager.addProtocolEndpoints(endpointManager);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to all WebSockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::sendToAllWebSockets(const uint8_t* pBuf, uint32_t bufLen)
{
	_connManager.sendToAllWebSockets(pBuf, bufLen);
}
