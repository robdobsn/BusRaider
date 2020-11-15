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

#include <RdWebServer.h>
#include <RdWebHandlerStaticFiles.h>
#include <RdWebHandlerRestAPI.h>
#include <RdWebHandlerWS.h>
WebServer* WebServer::_pThisWebServer = NULL;

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
    _accessControlAllowOriginAll = configGetLong("allowOriginAll", 1) != 0;

    // REST API prefix
    _restAPIPrefix = configGetString("apiPrefix", "api/");

    // File server enable
    bool enableFileServer = configGetLong("fileServer", 1) != 0;

    // Num connection slots
    uint32_t numConnSlots = configGetLong("numConnSlots", 6);

    // Max websockets
    uint32_t maxWebSockets = configGetLong("maxWS", 3);

    // Ping interval ms
    uint32_t pingIntervalMs = configGetLong("wsPingMs", 1000);

    // Websocket protocol
    String webSocketProtocol = configGetString("wsPcol", "RICSerial");

    // Get Task settings
    UBaseType_t taskCore = configGetLong("taskCore", RdWebServerSettings::DEFAULT_TASK_CORE);
    BaseType_t taskPriority = configGetLong("taskPriority", RdWebServerSettings::DEFAULT_TASK_PRIORITY);
    uint32_t taskStackSize = configGetLong("taskStack", RdWebServerSettings::DEFAULT_TASK_SIZE_BYTES);

    // Get server send buffer max length
    uint32_t sendBufferMaxLen = configGetLong("sendMax", RdWebServerSettings::DEFAULT_SEND_BUFFER_MAX_LEN);

    // Setup server if required
    if (_webServerEnabled)
    {
        // Start server
        if (!_isWebServerSetup)
        {
            RdWebServerSettings settings(_port, numConnSlots, maxWebSockets, 
                    pingIntervalMs, webSocketProtocol, enableFileServer,
                    taskCore, taskPriority, taskStackSize, sendBufferMaxLen);
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
    // Service
    _rdWebServer.service();
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
    // Offer to web server
    _rdWebServer.addProtocolEndpoints(endpointManager);
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

void WebServer::webSocketSetup(const String& websocketURL)
{
    // Add websocket handler
#ifdef DEBUG_WEBSOCKETS
    LOG_I(MODULE_PREFIX, "webSocketSetup wsPath %s", websocketURL.c_str());
#endif
    RdWebHandlerWS* pHandler = new RdWebHandlerWS(getProtocolEndpointManager(), websocketURL);
    _rdWebServer.addHandler(pHandler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over all WebSockets
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
