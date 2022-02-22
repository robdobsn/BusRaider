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
    
    // Server-side event handler (one-way text to browser)
    void enableServerSideEvents(const String& eventsURL);
    void sendServerSideEvent(const char* eventContent, const char* eventGroup);


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
    void addStaticResource(const WebServerResource *pResource, const char *pAliasPath = nullptr);
    void configChanged();
    void applySetup();
    void setupEndpoints();
    bool restAPIMatchEndpoint(const char* url, RdWebServerMethod method,
                    RdWebServerRestEndpoint& endpoint);
    void webSocketSetup();

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

    // Websockets
    std::vector<String> _webSocketConfigs;

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
