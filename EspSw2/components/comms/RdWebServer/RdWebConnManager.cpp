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
#include "RdWebResponder.h"
#include "ProtocolEndpointManager.h"
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include <Utils.h>
#include <NetworkSystem.h>

const static char* MODULE_PREFIX = "WebConnMgr";

// #define USE_THREAD_FOR_CLIENT_CONN_SERVICING

#ifdef USE_THREAD_FOR_CLIENT_CONN_SERVICING
#define RD_WEB_CONN_STACK_SIZE 5000
#endif

#define DEBUG_WEB_CONN_MANAGER
// #define DEBUG_WEB_SERVER_HANDLERS

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
	_webConnections.resize(_webServerSettings._numConnSlots);

	// Create queue for new connections
	_newConnQueue = xQueueCreate(_newConnQueueMaxLen, sizeof(struct netconn*));

#ifdef USE_THREAD_FOR_CLIENT_CONN_SERVICING
	// Start task to service connections
	xTaskCreatePinnedToCore(&clientConnHandlerTask,"clientConnTask", RD_WEB_CONN_STACK_SIZE, this, 6, NULL, 0);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::service()
{
#ifndef USE_THREAD_FOR_CLIENT_CONN_SERVICING
	serviceConnections();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client Connection Handler Task
// Handles client connections received on a queue and processes their HTTP requests
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::clientConnHandlerTask(void* pvParameters) 
{
#ifdef USE_THREAD_FOR_CLIENT_CONN_SERVICING
	// Get pointer to specific RdWebServer object
	RdWebConnManager* pConnMgr = (RdWebConnManager*)pvParameters;

	// Handle connection
	const static char* MODULE_PREFIX = "clientConnTask";

	// Debug
	LOG_I(MODULE_PREFIX, "clientConnHandlerTask starting");

	// Service connections
	while(1)
    {
		pConnMgr->serviceConnections();
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Connections
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::serviceConnections() 
{
	// Service existing connections or close them if inactive
	for (RdWebConnection& webConn : _webConnections)
	{
		// Service connection
		webConn.service();
	}

	// Get any new connection from queue
	struct netconn* pNewConnection = NULL;
	if (xQueueReceive(_newConnQueue, &pNewConnection, 1) == pdTRUE)
	{
		if(pNewConnection) 
		{
			// Put the connection into our connection list if we can
			if (!accommodateConnection(pNewConnection))
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
	LOG_I(MODULE_PREFIX, "accommodateConnection new webRequest pConn %lx", (unsigned long)pNewConnection);
#endif

	// Place new connection in slot - after this point the WebConnection is responsible for closing
	_webConnections[slotIdx].setNewConn(pNewConnection, this, _webServerSettings._sendBufferMaxLen);
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

void RdWebConnManager::addRestAPIEndpoints(RestAPIEndpointManager* pEndpointManager)
{
	// Take semaphore
	if (xSemaphoreTake(_endpointsMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
		return;
	
	// Record endpointManager
	_pEndpointManager = pEndpointManager;

	// Release semaphore
	xSemaphoreGive(_endpointsMutex);
}

void RdWebConnManager::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
	// For each possible websocket generate a channelID
	_webSocketProtocolChannelIDs.resize(_webServerSettings._maxWebSockets);
	for (uint32_t& chanId : _webSocketProtocolChannelIDs)
	{
	    chanId = endpointManager.registerChannel(_webServerSettings._wsProtocol.c_str(), 
	            std::bind(&RdWebConnManager::sendWebSocketMsg, this, std::placeholders::_1),
    	        "WS",
	            std::bind(&RdWebConnManager::readyToSendWebSocketMsg, this));
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::addHandler(RdWebHandler* pHandler)
{
	if (pHandler->isFileHandler() && !_webServerSettings._enableFileServer)
	{
#ifdef DEBUG_WEB_SERVER_HANDLERS
		LOG_I(MODULE_PREFIX, "addHandler NOT ADDING %s as file server disabled", pHandler->getName());
#endif
		return;
	}

#ifdef DEBUG_WEB_SERVER_HANDLERS
	LOG_I(MODULE_PREFIX, "addHandler %s", pHandler->getName());
#endif
	_webHandlers.push_back(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get new responder
// NOTE: this returns a new object or NULL
// NOTE: if a new object is returned the caller is responsible for deleting it when appropriate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponder* RdWebConnManager::getNewResponder(const RdWebRequestHeader& header, 
		const RdWebRequestParams& params, HttpStatusCode& statusCode)
{
	// Check limit on websockets
	RdWebRequestParams reqParams = params;
	if (header.reqConnType == REQ_CONN_TYPE_WEBSOCKET)
	{
		uint32_t protocolChannelID = 0;
		if (!allocateWebSocketChannelID(protocolChannelID))
		{
			statusCode = HTTP_STATUS_SERVICEUNAVAILABLE;
			return NULL;
		}
		reqParams.setProtocolChannelID(protocolChannelID);
	}

	// Iterate handlers to find one that gives a responder
	for (RdWebHandler* pHandler : _webHandlers)
	{
		if (pHandler)
		{
			// Get a responder
			RdWebResponder* pResponder = pHandler->getNewResponder(header, reqParams, 
						_webServerSettings);
			if (pResponder)
				return pResponder;
		}
	}
	statusCode = HTTP_STATUS_NOTFOUND;
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate WebSocket channelID
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::allocateWebSocketChannelID(uint32_t& protocolChannelID)
{
	std::list<uint32_t> possibleChannelIDs;
	for (uint32_t chId : _webSocketProtocolChannelIDs)
		possibleChannelIDs.push_back(chId);

	// Find an used channelIDs
	for (int i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;

		// Check if there's a responder
		RdWebResponder* pResponder = _webConnections[i].getResponder();
		if (!pResponder)
			continue;

		// Check if using channelID
		uint32_t usedChannelID = 0;
		if (pResponder->getProtocolChannelID(usedChannelID))
			possibleChannelIDs.remove(usedChannelID);
	}

	// Check for remaining channelID
	if (possibleChannelIDs.size() == 0)
		return false;
	protocolChannelID = possibleChannelIDs.front();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over a websocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::sendWebSocketMsg(ProtocolEndpointMsg& msg)
{
#ifdef DEBUG_WEBSOCKETS
    LOG_I(MODULE_PREFIX, "sendWebSocketMsg channelID %d, direction %s msgNum %d, len %d",
            msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());
#endif 

	// Find connection with this channelID
	for (int i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;

		// Check if there's a responder
		RdWebResponder* pResponder = _webConnections[i].getResponder();
		if (!pResponder)
			continue;

		// Check if using channelID
		uint32_t usedChannelID = 0;
		if (!pResponder->getProtocolChannelID(usedChannelID))
			continue;

		// Check for the channelID of the message
		if (usedChannelID == msg.getChannelID())
		{
			_webConnections[i].sendOnWebSocket(msg.getBuf(), msg.getBufLen());
			return true;
		}
	}
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if websocket is ready to send
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::readyToSendWebSocketMsg()
{
    return true;
}