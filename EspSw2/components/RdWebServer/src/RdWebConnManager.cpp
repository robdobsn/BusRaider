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
#include "RdWebHandlerWS.h"
#include "RdWebResponder.h"
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include <Utils.h>
#ifdef ESP8266
#include "ESP8266Utils.h"
#endif

const static char* MODULE_PREFIX = "WebConnMgr";

// #define USE_THREAD_FOR_CLIENT_CONN_SERVICING

#ifdef USE_THREAD_FOR_CLIENT_CONN_SERVICING
#define RD_WEB_CONN_STACK_SIZE 5000
#endif

#ifdef DEBUG_TRACE_HEAP_USAGE_WEB_CONN
#include "esp_heap_trace.h"
#endif

// #define DEBUG_WEB_CONN_MANAGER
// #define DEBUG_WEB_SERVER_HANDLERS
// #define DEBUG_WEBSOCKETS
// #define DEBUG_WEBSOCKETS_SEND_DETAIL
// #define DEBUG_NEW_RESPONDER

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebConnManager::RdWebConnManager()
{
    // Connection queue
    _newConnQueue = nullptr;

    // Mutex controlling endpoint access
    _endpointsMutex = xSemaphoreCreateMutex();
}

RdWebConnManager::~RdWebConnManager()
{
    if (_endpointsMutex)
        vSemaphoreDelete(_endpointsMutex);
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

#ifndef ESP8266
	// Create queue for new connections
	_newConnQueue = xQueueCreate(_newConnQueueMaxLen, sizeof(RdWebConnClientType));
#endif

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
#ifndef ESP8266
	RdWebConnClientType connClient;
	if (xQueueReceive(_newConnQueue, &connClient, 1) == pdTRUE)
	{
#ifdef DEBUG_TRACE_HEAP_USAGE_WEB_CONN
		heap_trace_start(HEAP_TRACE_LEAKS);
#endif
		// Put the connection into our connection list if we can
		if (!accommodateConnection(connClient))
		{
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
			// Debug
			LOG_W(MODULE_PREFIX, "serviceConn can't handle conn %d", connClient);
			close(connClient);			
#else
			// Debug
			LOG_W(MODULE_PREFIX, "serviceConn can't handle pConn %ld", (unsigned long)connClient);

			// Need to close the connection as we can't handle it
			netconn_close(connClient);
			netconn_delete(connClient);
#endif
		}
	}
#endif // ESP8266
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accommodate new connections if possible
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::accommodateConnection(RdWebConnClientType& connClient)
{
	// Handle the new connection if we can
	uint32_t slotIdx = 0;
	if (!findEmptySlot(slotIdx))
		return false;

	// Debug
#ifdef DEBUG_WEB_CONN_MANAGER
	LOG_I(MODULE_PREFIX, "accommodateConnection %ld", (unsigned long)connClient);
#endif

	// Place new connection in slot - after this point the WebConnection is responsible for closing
	_webConnections[slotIdx].setNewConn(connClient, this, _webServerSettings._sendBufferMaxLen);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Find an empty slot
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::findEmptySlot(uint32_t& slotIdx)
{
	// Check for inactive slots
	for (uint32_t i = 0; i < _webConnections.size(); i++)
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

bool RdWebConnManager::handleNewConnection(RdWebConnClientType& connClient)
{
#ifndef ESP8266
	// Add to queue for handling
	return xQueueSendToBack(_newConnQueue, &connClient, pdMS_TO_TICKS(10)) == pdTRUE;
#else // ESP8266
	// Get any new connection from queue
	if(pNewConnection) 
	{
		// Put the connection into our connection list if we can
		if (!accommodateConnection(pNewConnection))
		{
			// Debug
			LOG_W(MODULE_PREFIX, "Can't handle conn pConn %lx", (unsigned long)pNewConnection);
			return false;
		}
	}
	return true;
#endif // ESP8266
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::addHandler(RdWebHandler* pHandler)
{
	if (pHandler->isFileHandler() && !_webServerSettings._enableFileServer)
	{
#ifdef DEBUG_WEB_SERVER_HANDLERS
		LOG_I(MODULE_PREFIX, "addHandler NOT ADDING %s as file server disabled", pHandler->getName());
#endif
		return false;
	}
	else if (pHandler->isWebSocketHandler() && (_webServerSettings._maxWebSockets == 0))
	{
#ifdef DEBUG_WEB_SERVER_HANDLERS
		LOG_I(MODULE_PREFIX, "addHandler NOT ADDING %s as max websockets == 0", pHandler->getName());
#endif
		return false;
	}

#ifdef DEBUG_WEB_SERVER_HANDLERS
	LOG_I(MODULE_PREFIX, "addHandler %s", pHandler->getName());
#endif
	_webHandlers.push_back(pHandler);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get new responder
// NOTE: this returns a new object or nullptr
// NOTE: if a new object is returned the caller is responsible for deleting it when appropriate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponder* RdWebConnManager::getNewResponder(const RdWebRequestHeader& header, 
		const RdWebRequestParams& params, RdHttpStatusCode& statusCode)
{
	// Check limit on websockets
	RdWebRequestParams reqParams = params;
	if (header.reqConnType == REQ_CONN_TYPE_WEBSOCKET)
	{
		uint32_t protocolChannelID = 0;
		if (allocateWebSocketChannelID(protocolChannelID))
		{
			reqParams.setProtocolChannelID(protocolChannelID);
		}
		else
		{
			statusCode = HTTP_STATUS_SERVICEUNAVAILABLE;
			return nullptr;
		}
	}

	// Iterate handlers to find one that gives a responder
	for (RdWebHandler* pHandler : _webHandlers)
	{
		if (pHandler)
		{
			// Get a responder
			RdWebResponder* pResponder = pHandler->getNewResponder(header, reqParams, 
						_webServerSettings);

#ifdef DEBUG_NEW_RESPONDER
			LOG_I(MODULE_PREFIX, "getNewResponder url %s handlerType %s result %s",
					header.URL.c_str(), pHandler->getName(), 
					pResponder ? "OK" : "NoMatch");
#endif

			// Return responder if there is one
			if (pResponder)
				return pResponder;
		}
	}
	statusCode = HTTP_STATUS_NOTFOUND;
	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if websocket is ready to send
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::webSocketCanSend(uint32_t& protocolChannelID)
{
	// Find websocket responder corresponding to channel
	for (uint32_t i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;

		// Get responder
		RdWebResponder* pResponder = _webConnections[i].getResponder();
		if (!pResponder)
			continue;

		// Get channelID
		uint32_t usedChannelID = 0;
		if (!pResponder->getProtocolChannelID(usedChannelID))
			continue;

		// Check for channelID match
		if (usedChannelID == protocolChannelID)
		{
			return pResponder->readyForData();
		}
	}

	// If websocket doesn't exist (maybe it has just closed) then return true as this
	// message needs to be discarded so that queue is not blocked indefinitely
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to all WebSockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::webSocketSendMsg(const uint8_t* pBuf, uint32_t bufLen, 
			bool allWebSockets, uint32_t protocolChannelID)
{
	bool anyOk = false;
	for (uint32_t i = 0; i < _webConnections.size(); i++)
	{
#ifdef DEBUG_WEBSOCKETS_SEND_DETAIL
		{
			uint32_t debugChanId = 0;
			bool debugChanIdOk = false;
			if (_webConnections[i].getResponder())
				debugChanIdOk = _webConnections[i].getResponder()->getProtocolChannelID(debugChanId);
		    LOG_I(MODULE_PREFIX, "sendOnWebSocket webConn %d active %d responder %ld chanID %d ",
    	        i, 
				_webConnections[i].isActive(), 
				(unsigned long) _webConnections[i].getResponder(),
				debugChanIdOk ? debugChanId : -1);
		}
#endif 

		// Check active
		if (!_webConnections[i].isActive())
			continue;
		
		// Flag indicating we should send
		bool sendOnThisSocket = allWebSockets;
		if (!sendOnThisSocket)
		{
			// Get responder
			RdWebResponder* pResponder = _webConnections[i].getResponder();
			if (!pResponder)
				continue;

			// Get channelID
			uint32_t usedChannelID = 0;
			if (!pResponder->getProtocolChannelID(usedChannelID))
				continue;

			// Check for the channelID of the message
			if (usedChannelID == protocolChannelID)
				sendOnThisSocket = true;
		}

		// Send if appropriate
		if (sendOnThisSocket && (_webConnections[i].getHeader().reqConnType == REQ_CONN_TYPE_WEBSOCKET))
			anyOk |= _webConnections[i].sendOnWebSocket(pBuf, bufLen);
	}
	return anyOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate WebSocket channelID
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnManager::allocateWebSocketChannelID(uint32_t& protocolChannelID)
{
	// Get list of web-socket channel IDs
	RdWebHandler* pWsHandler = getWebSocketHandler();
	if (!pWsHandler)
		return false;
	std::list<uint32_t> possibleChannelIDs;
	pWsHandler->getChannelIDList(possibleChannelIDs);
	
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
// Send to all server-side events
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnManager::serverSideEventsSendMsg(const char* eventContent, const char* eventGroup)
{
	for (uint32_t i = 0; i < _webConnections.size(); i++)
	{
		// Check active
		if (!_webConnections[i].isActive())
			continue;
		
		if (_webConnections[i].getHeader().reqConnType == REQ_CONN_TYPE_EVENT)
			_webConnections[i].sendOnSSEvents(eventContent, eventGroup);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get websocket handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebHandler* RdWebConnManager::getWebSocketHandler()
{
	// Find websocket handler
	for (RdWebHandler* pHandler : _webHandlers)
	{
		if (!pHandler)
			continue;
		if (pHandler->isWebSocketHandler())
			return pHandler;
	}
	return nullptr;
}
