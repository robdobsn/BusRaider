/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Log Manager
// Handles logging to different destinations
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <LogManager.h>

class ConfigBase;
class RestAPIEndpointManager;
class APISourceInfo;
class LogManager : public SysModBase
{
public:
    LogManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, 
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
};
