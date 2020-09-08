/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "RdWebConnManager.h"
#include "RdWebConnection.h"
#include "RdWebHandler.h"
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include <Utils.h>
#ifdef USE_IDF_V4_1_NETIF_METHODS
#else
#include <tcpip_adapter.h>
#endif

const static char* MODULE_PREFIX = "WebConnMgr";

// #define DEBUG_WEB_CONN_MANAGER 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebConnManager::RdWebConnManager()
{
    // Endpoint manager
    _pEndpointManager = NULL;

    // Mutex controlling endpoint access
    _endpointsMutex = xSemaphoreCreateMutex();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::setup(RdWebServerSettings& settings)
{
	// Store settings
    _webServerSettings = settings;

	// Create slots
	_webConnections.resize(_webServerSettings.numConnSlots);

	// Create queue for new connections
	_newConnQueue = xQueueCreate(_newConnQueueMaxLen, sizeof(struct netconn*));

	// Start task to service connections
	xTaskCreatePinnedToCore(&clientConnHandlerTask,"clientConnTask", 5000, this, 6, NULL, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client Connection Handler Task
// Handles client connections received on a queue and processes their HTTP requests
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::clientConnHandlerTask(void* pvParameters) 
{
	// Get pointer to specific RdWebServer object
	RdWebConnManager* pConnMgr = (RdWebConnManager*)pvParameters;

	// Handle connection
	const static char* MODULE_PREFIX = "clientConnTask";

	// Debug
	LOG_I(MODULE_PREFIX, "clientConnHandlerTask starting");

	// Handle connections
	while(1)
    {
		// Service existing connections or close them if inactive
		for (RdWebConnection& webConn : pConnMgr->_webConnections)
		{
			// Service connection
			webConn.service();
		}

		// Get any new connection from queue
        struct netconn* pNewConnection = NULL;
		if (xQueueReceive(pConnMgr->_newConnQueue, &pNewConnection, 1) == pdTRUE)
		{
			if(pNewConnection) 
			{
				// Put the connection into our connection list if we can
				if (!pConnMgr->accommodateConnection(pNewConnection))
				{
					// Debug
					LOG_W(MODULE_PREFIX, "Can't handle conn pConn %lx", (unsigned long)pNewConnection);

					// Need to close the connection as we can't handle it
					netconn_close(pNewConnection);
					netconn_delete(pNewConnection);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accommodate new connections if possible
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::accommodateConnection(struct netconn* pNewConnection)
{
	// Handle the new connection if we can
	uint32_t slotIdx = 0;
	if (!findEmptySlot(slotIdx))
		return false;

	// Debug
#ifdef DEBUG_WEB_CONN_MANAGER
	LOG_W(MODULE_PREFIX, "accommodateConnection new webRequest pConn %lx", (unsigned long)pNewConnection);
#endif

	// Place new connection in slot - after this point the WebConnection is responsible for closing
	_webConnections[slotIdx].setNewConn(pNewConnection, this);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Find an empty slot
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::findEmptySlot(uint32_t& slotIdx)
{
	// Check for inactive slots
	for (int i = 0; i < _webConnections.size(); i++)
	{
		// Check
		if (_webConnections[i].isActive())
			continue;
		
		// Return inactive
		slotIdx = i;
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Count active websocket connections
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebConnManager::countWebSockets()
{
	// Count
	uint32_t numWS = 0;
	for (int i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;
		
		if (_webConnections[i].getHeader().reqConnType == REQ_CONN_TYPE_WEBSOCKET)
			numWS++;
	}
	return numWS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Incoming connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::handleNewConnection(struct netconn* pNewConnection)
{
	// Add to queue for handling
	return xQueueSendToBack(_newConnQueue, &pNewConnection, pdMS_TO_TICKS(1000)) == pdTRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::addEndpoints(RestAPIEndpointManager* pEndpointManager)
{
	// Take semaphore
	xSemaphoreTake(_endpointsMutex, pdMS_TO_TICKS(1000));
	
	// Record endpointManager
	_pEndpointManager = pEndpointManager;

	// Release semaphore
	xSemaphoreGive(_endpointsMutex);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::addHandler(RdWebHandler* pHandler)
{
	LOG_I(MODULE_PREFIX, "addHandler %s", pHandler->getName());
	_webHandlers.push_back(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get new responder
// NOTE: this returns a new object or NULL
// NOTE: if a new object is returned the caller is responsible for deleting it when appropriate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponder* RdWebConnManager::getNewResponder(const RdWebRequestHeader& header, const RdWebRequestParams& params)
{
	// Check limit on websockets
	if (header.reqConnType == REQ_CONN_TYPE_WEBSOCKET)
	{
		if (countWebSockets() >= _webServerSettings.maxWebSockets)
			return NULL;
	}

	// Iterate handlers to find one that gives a responder
	for (RdWebHandler* pHandler : _webHandlers)
	{
		if (pHandler)
		{
			// Get a responder
			RdWebResponder* pResponder = pHandler->getNewResponder(header, params, 
						_webServerSettings);
			if (pResponder)
				return pResponder;
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to all WebSockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::sendToAllWebSockets(const uint8_t* pBuf, uint32_t bufLen)
{
	for (int i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;
		
		if (_webConnections[i].getHeader().reqConnType == REQ_CONN_TYPE_WEBSOCKET)
			_webConnections[i].sendOnWebSocket(pBuf, bufLen);
	}
}
