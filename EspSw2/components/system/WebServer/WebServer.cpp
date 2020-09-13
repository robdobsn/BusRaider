/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WebServer
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <WebServer.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>
#include "WebServerResource.h"
#include "ProtocolEndpointManager.h"
#include <NetworkSystem.h>

#ifdef USE_ASYNC_WEB_SERVER
#include <AsyncTCP.h>
#include <AsyncWebServer.h>
#include <AsyncStaticFileHandler.h>
#else
#include <RdWebServer.h>
#include <RdWebHandlerStaticFiles.h>
#include <RdWebHandlerRestAPI.h>
#include <RdWebHandlerWS.h>
WebServer* WebServer::_pThisWebServer = NULL;
#endif

static const char* MODULE_PREFIX = "WebServer";

// #define DEBUG_WEBSOCKETS 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WebServer::WebServer(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig) 
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Config variables
    _webServerEnabled = false;
    _accessControlAllowOriginAll = true;
    _port = 80;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;

    // Is setup
    _isWebServerSetup = false;

    // Singleton
    _pThisWebServer = this;
}

WebServer::~WebServer()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::setup()
{
    // Hook change of config
    configRegisterChangeCallback(std::bind(&WebServer::configChanged, this));

    // Apply config
    applySetup();
}

void WebServer::configChanged()
{
    // Reset config
    LOG_D(MODULE_PREFIX, "configChanged");
    applySetup();
}

void WebServer::applySetup()
{
    // Enable
    _webServerEnabled = configGetLong("enable", 0) != 0;

    // Port
    _port = configGetLong("webServerPort", 80);

    // Access control allow origin all
    _accessControlAllowOriginAll = configGetLong("allowOriginAll", 0) != 0;

    // REST API prefix
    _restAPIPrefix = configGetString("apiPrefix", "api/");

    // Num connection slots
    uint32_t numConnSlots = configGetLong("numConnSlots", 6);

    // Max websockets
    uint32_t maxWebSockets = configGetLong("maxWS", 3);

    // Ping interval ms
    uint32_t pingIntervalMs = configGetLong("wsPingMs", 1000);

    // Websocket protocol
    _webSocketProtocol = configGetString("wsPcol", "RICSerial");

    // Setup server if required
    if (_webServerEnabled)
    {
        // Start server
        if (!_isWebServerSetup)
        {
            RdWebServerSettings settings(_port, numConnSlots, maxWebSockets, pingIntervalMs);
            _rdWebServer.setup(settings);
        }
        _isWebServerSetup = true;

    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::beginServer()
{
    // Add headers
    if (_accessControlAllowOriginAll)
        _rdWebServer.addResponseHeader({"Access-Control-Allow-Origin", "*"});
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    setupEndpoints();
}

void WebServer::setupEndpoints()
{
    // Handle REST endpoints
    LOG_I(MODULE_PREFIX, "setupEndpoints");
    RdWebHandlerRestAPI* pHandler = new RdWebHandlerRestAPI(getRestAPIEndpoints(), _restAPIPrefix);
    _rdWebServer.addHandler(pHandler);
    }

void WebServer::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
    // Register as a channel for protocol messages - websocket
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints webSocketProtocol %s", _webSocketProtocol.c_str());
    _protocolEndpointID = endpointManager.registerChannel(_webSocketProtocol.c_str(), 
            std::bind(&WebServer::sendWebSocketMsg, this, std::placeholders::_1),
            "WS",
            std::bind(&WebServer::readyToSendWebSocketMsg, this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static Resources
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add resources to the web server
void WebServer::addStaticResources(const WebServerResource *pResources, int numResources)
{
}

void WebServer::addStaticResource(const WebServerResource *pResource, const char *pAliasPath)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static Files
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::serveStaticFiles(const char* baseUrl, const char* baseFolder, const char* cacheControl)
{
    // Handle file systems
    LOG_I(MODULE_PREFIX, "serveStaticFiles url %s folder %s", baseUrl, baseFolder);
    RdWebHandlerStaticFiles* pHandler = new RdWebHandlerStaticFiles(baseUrl, baseFolder, cacheControl);
    _rdWebServer.addHandler(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Async Events
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::enableAsyncEvents(const String& eventsURL)
{
}

void WebServer::sendAsyncEvent(const char* eventContent, const char* eventGroup)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Web sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::webSocketOpen(const String& websocketURL)
{
    // Add websocket handler
    LOG_I(MODULE_PREFIX, "webSocketHandler wsPath %s", websocketURL.c_str());
    RdWebHandlerWS* pHandler = new RdWebHandlerWS(websocketURL, webSocketCallback);
    _rdWebServer.addHandler(pHandler);
}

void WebServer::webSocketSend(const uint8_t* pBuf, uint32_t len)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Websocket callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// handles websocket events
void WebServer::webSocketCallback(RdWebSocketEventCode eventCode, const uint8_t* pBuf, uint32_t bufLen)
{
#ifdef DEBUG_WEBSOCKETS
	const static char* MODULE_PREFIX = "wsCB";
#endif
    if (!_pThisWebServer)
    {
        return; 
    }

	switch(eventCode) 
    {
		case WEBSOCKET_EVENT_CONNECT:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "connected!");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_EXTERNAL:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "sent a disconnect message");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_INTERNAL:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "was disconnected");
#endif
			break;
        }
		case WEBSOCKET_EVENT_DISCONNECT_ERROR:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "was disconnected due to an error");
#endif
			break;
        }
		case WEBSOCKET_EVENT_TEXT:
        {
            // Send the message to the ProtocolEndpointManager
            if (_pThisWebServer->getProtocolEndpointManager() && (pBuf != NULL))
                _pThisWebServer->getProtocolEndpointManager()->handleInboundMessage(_pThisWebServer->_protocolEndpointID, (uint8_t*) pBuf, bufLen);
#ifdef DEBUG_WEBSOCKETS
            String msgText;
            if (pBuf)
                Utils::strFromBuffer(pBuf, bufLen, msgText);
			LOG_I(MODULE_PREFIX, "sent text message of size %i content %s", bufLen, msgText.c_str());
#endif
			break;
        }
		case WEBSOCKET_EVENT_BINARY:
        {
            // Send the message to the ProtocolEndpointManager
            if (_pThisWebServer->getProtocolEndpointManager() && (pBuf != NULL))
                _pThisWebServer->getProtocolEndpointManager()->handleInboundMessage(_pThisWebServer->_protocolEndpointID, (uint8_t*) pBuf, bufLen);
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "sent binary message of size %i", bufLen);
#endif
			break;
        }
		case WEBSOCKET_EVENT_PING:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "pinged us with message of size %i", bufLen);
#endif
			break;
        }
		case WEBSOCKET_EVENT_PONG:
        {
#ifdef DEBUG_WEBSOCKETS
			LOG_I(MODULE_PREFIX, "responded to the ping");
#endif
		    break;
        }
        default:
        {
            break;
        }
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over WebSocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WebServer::sendWebSocketMsg(ProtocolEndpointMsg& msg)
{
#ifdef DEBUG_WEBSOCKETS
    LOG_I(MODULE_PREFIX, "sendWebSocketMsg channelID %d, direction %s msgNum %d, len %d",
            msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());
#endif 
    _rdWebServer.sendToAllWebSockets(msg.getBuf(), msg.getBufLen());

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready to send indicator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WebServer::readyToSendWebSocketMsg()
{
    return true;
}
