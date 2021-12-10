/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StreamManager
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SysModBase.h"
#include "ConfigBase.h"

class RestAPIEndpointManager;
class APISourceInfo;

class StreamManager : public SysModBase
{
public:
    StreamManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Upload file to file system
    virtual bool fileStreamDataBlock(FileStreamBlock& fileStreamBlock) override final;

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

private:

    // API stream complete
    void apiTestStreamComplete(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // API stream block
    bool apiTestStreamBlock(const String& req, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo);

    // API stream isReady
    bool apiTestStreamIsReady(const APISourceInfo& sourceInfo);

    // Time of last request
    uint32_t _lastReqMs;
};
