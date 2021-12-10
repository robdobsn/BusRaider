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
#include "FileStreamBlock.h"

class RestAPIEndpointManager;
class APISourceInfo;

class ESPOTAUpdate : public SysModBase
{
public:
    ESPOTAUpdate(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Check if update in progress
    virtual bool isBusy() override final
    {
        return _otaDirectInProgress;
    }

    // Start/Data/Cancel methods
    virtual bool fileStreamStart(const char* fileName, size_t fileLen) override final;
    virtual bool fileStreamDataBlock(FileStreamBlock& fileStreamBlock) override final;
    virtual bool fileStreamCancelEnd(bool isNormalEnd) override final;

    // Get debug string
    virtual String getDebugJSON() override final;

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
    uint64_t _fwUpdateTimeStartedUs;
    uint64_t _fwUpdateBeginTimeUs;
    uint64_t _fwUpdateWriteTimeUs;
    uint32_t _fwUpdateBytes;
    float _fwUpdateLastRate;
    uint16_t _blockSizeLast;

private:
    // Handle received data
    void onDataReceived(uint8_t *pDataReceived, size_t dataReceivedLen);

    // API ESP Firmware update
    void apiESPFirmwarePart(const String& req, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo);
    void apiESPFirmwareUpdateDone(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // Direct firmware update
    virtual void fwUpdateAPIPart(FileStreamBlock& fileStreamBlock);
    void fwUpdateAPIFinal();
    bool firmwareUpdateEnd();
};
