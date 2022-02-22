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

// #define DEBUG_WEBSOCKETS

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

    // Ping interval ms
    uint32_t pingIntervalMs = configGetLong("wsPingMs", 1000);

    // Websockets
    configGetArrayElems("websockets", _webSocketConfigs);

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
            RdWebServerSettings settings(_port, numConnSlots, _webSocketConfigs.size() > 0, 
                    pingIntervalMs, enableFileServer,
                    taskCore, taskPriority, taskStackSize, sendBufferMaxLen,
                    ProtocolEndpointManager::CHANNEL_ID_REST_API);
            _rdWebServer.setup(settings);
        }
        _isWebServerSetup = true;
    }

#ifdef FEATURE_WEB_SOCKETS
    // Serve websockets
    webSocketSetup();
#endif

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
    LOG_I(MODULE_PREFIX, "setupEndpoints serverEnabled %s port %d apiPrefix %s accessControlAllowOriginAll %s", 
            _webServerEnabled ? "Y" : "N", _port, 
            _restAPIPrefix.c_str(), _accessControlAllowOriginAll ? "Y" : "N");
    RdWebHandlerRestAPI* pHandler = new RdWebHandlerRestAPI(_restAPIPrefix,
                std::bind(&WebServer::restAPIMatchEndpoint, this, std::placeholders::_1, 
                        std::placeholders::_2, std::placeholders::_3));
    if (!_rdWebServer.addHandler(pHandler))
        delete pHandler;
}

void WebServer::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
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
    RdWebHandlerStaticFiles* pHandler = new RdWebHandlerStaticFiles(baseUrl, baseFolder, cacheControl, "index.html");
    bool handlerAddOk = _rdWebServer.addHandler(pHandler);
    LOG_I(MODULE_PREFIX, "serveStaticFiles url %s folder %s addResult %s", baseUrl, baseFolder, 
                handlerAddOk ? "OK" : "FILE SERVER DISABLED");
    if (!handlerAddOk)
        delete pHandler;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Async Events
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::enableServerSideEvents(const String& eventsURL)
{
}

void WebServer::sendServerSideEvent(const char* eventContent, const char* eventGroup)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Web sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebServer::webSocketSetup()
{
    // Add websocket handler
#ifdef DEBUG_WEBSOCKETS
    LOG_I(MODULE_PREFIX, "webSocketSetup num websocket configs %d", _webSocketConfigs.size());
#endif
    ProtocolEndpointManager* pEndpointManager = getProtocolEndpointManager();
    if (!pEndpointManager)
        return;
    
    // Create websockets
    for (uint32_t wsIdx = 0; wsIdx < _webSocketConfigs.size(); wsIdx++)
    {
        // Get config
        ConfigBase jsonConfig = _webSocketConfigs[wsIdx];

        // Setup WebHandler for Websockets    
        RdWebHandlerWS* pHandler = new RdWebHandlerWS(jsonConfig, 
                std::bind(&ProtocolEndpointManager::canAcceptInbound, pEndpointManager, 
                        std::placeholders::_1),
                std::bind(&ProtocolEndpointManager::handleInboundMessage, pEndpointManager, 
                        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                );
        if (!pHandler)
            continue;

        // Add handler
        if (!_rdWebServer.addHandler(pHandler))
        {
            delete pHandler;
            continue;
        }

        // Register a channel  with the protocol endpoint manager
        // for each possible connection
        uint32_t maxConn = jsonConfig.getLong("maxConn", 1);
        for (uint32_t possConnIdx = 0; possConnIdx < maxConn; possConnIdx++)
        {
            String interfaceName = jsonConfig.getString("pfix", "ws");
            String wsName = interfaceName + "_" + possConnIdx;
            String protocol = jsonConfig.getString("pcol", "RICSerial");
            uint32_t wsChanID = getProtocolEndpointManager()->registerChannel(
                    protocol.c_str(), 
                    interfaceName.c_str(),
                    wsName.c_str(),
                    [this](ProtocolEndpointMsg& msg) { 
                        return _rdWebServer.sendMsg(msg.getBuf(), msg.getBufLen(), 
                                false, msg.getChannelID());
                    },
                    [this](uint32_t channelID, bool& noConn) {
                        return _rdWebServer.canSend(channelID, noConn); 
                    },
                    WS_INBOUND_BLOCK_MAX_DEFAULT,
                    WS_INBOUND_QUEUE_MAX_DEFAULT,
                    WS_OUTBOUND_BLOCK_MAX_DEFAULT,
                    WS_OUTBOUND_QUEUE_MAX_DEFAULT);

            // Set into the websocket handler so channel IDs match up
            pHandler->setupWebSocketChannelID(possConnIdx, wsChanID);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback used to match endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WebServer::restAPIMatchEndpoint(const char* url, RdWebServerMethod method,
                    RdWebServerRestEndpoint& endpoint)
{
    // Check valid
    if (!getRestAPIEndpoints())
        return false;

    // Rest API match
    RestAPIEndpointDef::EndpointMethod restAPIMethod = convWebToRESTAPIMethod(method);
    RestAPIEndpointDef* pEndpointDef = getRestAPIEndpoints()->getMatchingEndpointDef(url, restAPIMethod);
    if (pEndpointDef)
    {
        endpoint.restApiFn = pEndpointDef->_callbackMain;
        endpoint.restApiFnBody = pEndpointDef->_callbackBody;
        endpoint.restApiFnChunk = pEndpointDef->_callbackChunk;
        endpoint.restApiFnIsReady = pEndpointDef->_callbackIsReady;
        return true;
    }
    return false;
}