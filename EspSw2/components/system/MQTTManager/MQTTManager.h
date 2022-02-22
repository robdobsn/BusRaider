/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MQTT Manager
// Handles state of MQTT connections
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <RdMQTTClient.h>

class ConfigBase;
class RestAPIEndpointManager;
class APISourceInfo;
class MQTTManager : public SysModBase
{
public:
    MQTTManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, 
                ConfigBase* pMutableConfig);

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager) override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

    // Get debug string
    virtual String getDebugJSON() override final;

private:
    // MQTT client
    RdMQTTClient _mqttClient;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Protocol message handling
    static const uint32_t MQTT_INBOUND_BLOCK_MAX_DEFAULT = 2000;
    static const uint32_t MQTT_INBOUND_QUEUE_MAX_DEFAULT = 5;
    static const uint32_t MQTT_OUTBOUND_BLOCK_MAX_DEFAULT = 2000;
    static const uint32_t MQTT_OUTBOUND_QUEUE_MAX_DEFAULT = 5;

    // Helpers
    bool sendMQTTMsg(const String& topicName, ProtocolEndpointMsg& msg);
};
