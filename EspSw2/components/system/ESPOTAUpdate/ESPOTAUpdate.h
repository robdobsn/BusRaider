/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Over-The-Air (OTA) Firmware Update
// Handles OTA
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Utils.h>
#include <ConfigBase.h>
#include <SysModBase.h>
#include "esp_ota_ops.h"
#include "FileBlockInfo.h"

class RestAPIEndpointManager;

class ESPOTAUpdate : public SysModBase
{
public:
    ESPOTAUpdate(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Check if update in progress
    virtual bool isBusy() override final
    {
        return _otaDirectInProgress;
    }

    // Is this a firmware update module SysMod
    virtual bool isFirmwareUpdateModule()
    {
        return true;
    }

    // Firmware update method
    virtual bool firmwareUpdateStart(const char* fileName, size_t fileLen) override final;
    virtual bool firmwareUpdateBlock(uint32_t filePos, const uint8_t *pBlock, size_t blockLen) override final;
    virtual bool firmwareUpdateEnd() override final;

    // Get debug string
    virtual String getDebugStr() override final;

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

private:

    // Direct enabled
    bool _otaDirectEnabled;
    static const int TIME_TO_WAIT_BEFORE_RESTART_MS = 1000;

    // Direct update restart pending
    bool _directUpdateRestartPending;
    int _directUpdateRestartPendingStartMs;

    // Direct update vars
    bool _otaDirectInProgress;
    esp_ota_handle_t _otaDirectUpdateHandle;

    // Firmware update
    uint64_t _fwUpdateStartTimeUs;
    uint64_t _fwUpdateWriteTimeUs;
    uint32_t _fwUpdateBytes;

private:
    // Handle received data
    void onDataReceived(uint8_t *pDataReceived, size_t dataReceivedLen);

    // API ESP Firmware update
    void apiESPFirmwarePart(const String& req, FileBlockInfo& fileBlockInfo);
    void apiESPFirmwareUpdateDone(const String &reqStr, String &respStr);

    // Direct firmware update
    virtual void fwUpdateAPIPart(FileBlockInfo& fileBlockInfo);
    void fwUpdateAPIFinal();
};
