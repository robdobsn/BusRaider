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
#ifndef ESP8266
#include "lwip/api.h"
#include "lwip/sockets.h"
#endif
#include <Utils.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
#include "esp_netif.h"
#else
#ifndef ESP8266
#include "tcpip_adapter.h"
#endif
#endif

static const char *MODULE_PREFIX = "RdWebServer";

#define INFO_WEB_SERVER_SETUP
// #define DEBUG_NEW_CONNECTION

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebServer::RdWebServer()
{
#ifdef ESP8266
    _pWiFiServer = NULL;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup Web
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::setup(RdWebServerSettings& settings)
{
	// Settings
    _webServerSettings = settings;
    
#ifdef INFO_WEB_SERVER_SETUP
    LOG_I(MODULE_PREFIX, "setup port %d numConnSlots %d maxWS %d enableFileServer %d", 
            settings._serverTCPPort, settings._numConnSlots, 
            settings._maxWebSockets, settings._enableFileServer);
#endif

    // Start network interface if not already started
#ifndef ESP8266
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
        if (esp_netif_init() != ESP_OK) {
            LOG_E(MODULE_PREFIX, "could not start netif");
        }
#else
        tcpip_adapter_init();
#endif
#else
    if (!_pWiFiServer)
        _pWiFiServer = new WiFiServer(settings._serverTCPPort);
    if (!_pWiFiServer)
        return;
    _pWiFiServer->begin();
#endif

	// Setup connection manager
	_connManager.setup(_webServerSettings);

#ifndef ESP8266
	// Start task to handle listen for connections
	xTaskCreatePinnedToCore(&socketListenerTask,"socketLstnTask", 
            settings._taskStackSize,
            this, 
            settings._taskPriority, 
            NULL, 
            settings._taskCore);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::service()
{
#ifdef ESP8266
    // Check for new connection
    if (_pWiFiServer)
    {
        WiFiClient client = _pWiFiServer->available();
        if (client)
        {
            WiFiClient* pClient = new WiFiClient(client);
            // Handle the connection
#ifdef DEBUG_NEW_CONNECTION
            LOG_I(MODULE_PREFIX, "New client");
#endif
            if (!_connManager.handleNewConnection(pClient))
            {
                LOG_W(MODULE_PREFIX, "No room so client stopped");
                pClient->stop();
                delete pClient;
            }
        }
    }
#endif
    // Service connection manager
    _connManager.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebServer::addHandler(RdWebHandler* pHandler)
{
    return _connManager.addHandler(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Web Server Task
// Listen for connections and add to queue for handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266
void RdWebServer::socketListenerTask(void* pvParameters) 
{
	// Get pointer to specific RdWebServer object
	RdWebServer* pWS = (RdWebServer*)pvParameters;

    // Loop forever
    while (1)
    {
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
        // Create socket
        int socketId = socket(AF_INET , SOCK_STREAM , 0);
        if (socketId < 0)
        {
            LOG_W(MODULE_PREFIX, "socketListenerTask failed to create socket");
            vTaskDelay(WEB_SERVER_SOCKET_RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        // Form address to be used in bind call - IPV4 assumed
        struct sockaddr_in bindAddr;
        memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(pWS->_webServerSettings._serverTCPPort);

        // Bind to IP and port
        int bind_err = bind(socketId, (struct sockaddr *)&bindAddr, sizeof(bindAddr));
        if (bind_err != 0)
        {
            LOG_W(MODULE_PREFIX, "socketListenerTask failed to bind on port %d errno %d",
                                pWS->_webServerSettings._serverTCPPort, errno);
            shutdown(socketId, 0);
            close(socketId);
            vTaskDelay(WEB_SERVER_SOCKET_RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        // Listen for clients
        int listen_error = listen(socketId, pWS->_webServerSettings._numConnSlots);
        if (listen_error != 0)
        {
            LOG_W(MODULE_PREFIX, "socketListenerTask failed to listen errno %d", errno);
            shutdown(socketId, 0);
            close(socketId);
            vTaskDelay(WEB_SERVER_SOCKET_RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }
        LOG_I(MODULE_PREFIX, "socketListenerTask listening");

        // Wait for connection
        while (true)
        {
            // Client info
            struct sockaddr_storage clientInfo;
            socklen_t clientInfoLen = sizeof(clientInfo);
            int sockClient = accept(socketId, (struct sockaddr *)&clientInfoLen, &clientInfoLen);
            if(sockClient < 0)
            {
                LOG_W(MODULE_PREFIX, "socketListenerTask failed to accept %d", errno);
                bool socketReconnNeeded = false;
                switch(errno)
                {
                    case ENETDOWN:
                    case EPROTO:
                    case ENOPROTOOPT:
                    case EHOSTDOWN:
                    case EHOSTUNREACH:
                    case EOPNOTSUPP:
                    case ENETUNREACH:
                        vTaskDelay(WEB_SERVER_SOCKET_RETRY_DELAY_MS / portTICK_PERIOD_MS);
                        break;
                    case EWOULDBLOCK:
                        break;
                    default:
                        socketReconnNeeded = true;
                        break;
                }
                if (socketReconnNeeded)
                    break;
                continue;
            }
            else
            {
#ifdef DEBUG_NEW_CONNECTION
                LOG_I(MODULE_PREFIX, "socketListenerTask newConn handle %d", sockClient);
#endif
                // Handle the connection - if this returns true then it is the 
                // responsibility of the client to close the connection
                if (!pWS->_connManager.handleNewConnection(sockClient))
                {
                    shutdown(sockClient, 0);
                    close(sockClient);
                }
            }
        }

        // Listener exited
        shutdown(socketId, 0);
        close(socketId);
        LOG_E(MODULE_PREFIX,"socketListenerTask socket closed");

        // Delay hoping networking recovers
        vTaskDelay(WEB_SERVER_SOCKET_RETRY_DELAY_MS / portTICK_PERIOD_MS);
    }

#else
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
            LOG_I(MODULE_PREFIX, "new connection netconn %s", RdWebInterface::espIdfErrToStr(errCode));
#endif
            if ((errCode == ERR_OK) && pNewConnection)
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
#endif
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::addResponseHeader(RdJson::NameValuePair headerInfo)
{
    _connManager.addResponseHeader(headerInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if websocket can send
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebServer::webSocketCanSend(uint32_t protocolChannelID)
{
	return _connManager.webSocketCanSend(protocolChannelID);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send on a websocket (or all websockets)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebServer::webSocketSendMsg(const uint8_t* pBuf, uint32_t bufLen, 
                bool allWebSockets, uint32_t protocolChannelID)
{
	return _connManager.webSocketSendMsg(pBuf, bufLen, allWebSockets, protocolChannelID);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to all server-side events
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebServer::serverSideEventsSendMsg(const char* eventContent, const char* eventGroup)
{
	_connManager.serverSideEventsSendMsg(eventContent, eventGroup);
}
