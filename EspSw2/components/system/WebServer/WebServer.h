/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WebServer
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include <RdWebInterface.h>

class WebServerResource;
class ProtocolEndpointMsg;

#include "RdWebServer.h"

class WebServer : public SysModBase
{
public:
    // Constructor/destructor
    WebServer(const char* pModuleName, ConfigBase& defaultConfig, 
            ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);
    virtual ~WebServer();

    // Begin
    void beginServer();

    // Add resources to the web server
    void addStaticResources(const WebServerResource *pResources, int numResources);
    void serveStaticFiles(const char* baseUrl, const char* baseFolder, const char* cacheControl = NULL);
    
    // Async event handler (one-way text to browser)
    void enableAsyncEvents(const String& eventsURL);
    void sendAsyncEvent(const char* eventContent, const char* eventGroup);

    // Web sockets
    void webSocketSetup(const String& websocketURL);

protected:
    // Setup
    virtual void setup();

    // Service - called frequently
    virtual void service();

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager);

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager);
    
private:
    // Helpers
    void addStaticResource(const WebServerResource *pResource, const char *pAliasPath = NULL);
    void configChanged();
    void applySetup();
    void setupEndpoints();
    bool webSocketSendMsg(ProtocolEndpointMsg& msg);
    bool webSocketCanSend(uint32_t channelID);
    bool restAPIMatchEndpoint(const char* url, RdWebServerMethod method,
                    RdWebServerRestEndpoint& endpoint);

    // Server config
    bool _accessControlAllowOriginAll;
    bool _webServerEnabled;
    uint32_t _port;
    String _restAPIPrefix;

    // Web server setup
    bool _isWebServerSetup;

    // Server
    RdWebServer _rdWebServer;

    // Singleton
    static WebServer* _pThisWebServer;

    // Websocket protocol
    String _webSocketProtocol;

    // Websocket protocol handling
    static const uint32_t WS_INBOUND_BLOCK_MAX_DEFAULT = 1200;
    static const uint32_t WS_INBOUND_QUEUE_MAX_DEFAULT = 20;
    static const uint32_t WS_OUTBOUND_BLOCK_MAX_DEFAULT = 1200;
    static const uint32_t WS_OUTBOUND_QUEUE_MAX_DEFAULT = 5;

    // Mapping from web-server method to RESTAPI method enums
    RestAPIEndpointDef::EndpointMethod convWebToRESTAPIMethod(RdWebServerMethod method)
    {
        switch(method)
        {
            case WEB_METHOD_POST: return RestAPIEndpointDef::ENDPOINT_POST;
            case WEB_METHOD_PUT: return RestAPIEndpointDef::ENDPOINT_PUT;
            case WEB_METHOD_DELETE: return RestAPIEndpointDef::ENDPOINT_DELETE;
            default: return RestAPIEndpointDef::ENDPOINT_GET;
        }
    }
};
