/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TCPConnection
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "TCPConnManager.h"
#include "TCPConnection.h"
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include <Utils.h>
#ifdef USE_IDF_V4_1_NETIF_METHODS
#else
#include <tcpip_adapter.h>
#endif

const static char* MODULE_PREFIX = "TCPConnMgr";

// #define DEBUG_TCP_CONN_MANAGER 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TCPConnManager::TCPConnManager()
{
    // Endpoint manager
    _pEndpointManager = NULL;

    // Mutex controlling endpoint access
    _endpointsMutex = xSemaphoreCreateMutex();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnManager::setup(TCPConnSettings* pSettings, unsigned numSettings)
{
	// Store settings
    _tcpConnSettings.assign(pSettings, pSettings + numSettings);

	// Create slots
	_tcpConnections.resize(_tcpConnSettings.numConnSlots);

	// Create queue for new connections
	_newConnQueue = xQueueCreate(_newConnQueueMaxLen, sizeof(struct netconn*));

	// Start task to service connections
	xTaskCreatePinnedToCore(&clientConnHandlerTask,"clientConnTask", 5000, this, 6, NULL, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client Connection Handler Task
// Handles client connections received on a queue and processes their HTTP requests
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnManager::clientConnHandlerTask(void* pvParameters) 
{
	// Get pointer to specific RdWebServer object
	TCPConnManager* pConnMgr = (TCPConnManager*)pvParameters;

	// Handle connection
	const static char* MODULE_PREFIX = "clientConnTask";

	// Debug
	LOG_I(MODULE_PREFIX, "clientConnHandlerTask starting");

	// Handle connections
	while(1)
    {
		// Service existing connections or close them if inactive
		for (TCPConnection& tcpConn : pConnMgr->_tcpConnections)
		{
			// Service connection
			tcpConn.service();
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

bool TCPConnManager::accommodateConnection(struct netconn* pNewConnection)
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
	_tcpConnections[slotIdx].setNewConn(pNewConnection, this);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Find an empty slot
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TCPConnManager::findEmptySlot(uint32_t& slotIdx)
{
	// Check for inactive slots
	for (int i = 0; i < _tcpConnections.size(); i++)
	{
		// Check
		if (_tcpConnections[i].isActive())
			continue;
		
		// Return inactive
		slotIdx = i;
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Count active socket connections
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TCPConnManager::countActiveSockets()
{
	// Count
	uint32_t numActiveConn = 0;
	for (int i = 0; i < _tcpConnections.size(); i++)
	{
		// Check active
		if (!_tcpConnections[i].isActive())
			continue;
		numActiveConn++;
	}
	return numActiveConn;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Incoming connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TCPConnManager::handleNewConnection(struct netconn* pNewConnection)
{
	// Add to queue for handling
	return xQueueSendToBack(_newConnQueue, &pNewConnection, pdMS_TO_TICKS(1000)) == pdTRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnManager::addEndpoints(RestAPIEndpointManager* pEndpointManager)
{
	// Take semaphore
	xSemaphoreTake(_endpointsMutex, pdMS_TO_TICKS(1000));
	
	// Record endpointManager
	_pEndpointManager = pEndpointManager;

	// Release semaphore
	xSemaphoreGive(_endpointsMutex);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to all sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnManager::sendToAllSockets(const uint8_t* pBuf, uint32_t bufLen)
{
	for (int i = 0; i < _tcpConnections.size(); i++)
	{
		// Check active
		if (!_tcpConnections[i].isActive())
			continue;
		
		_tcpConnections[i].sendOnSocket(pBuf, bufLen);
	}
}
