/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Over-The-Air (OTA) Firmware Update
// Handles OTA
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESPOTAUpdate.h>
// #include "Update.h"
#include <RestAPIEndpointManager.h>
#include <Logger.h>
#include "esp_system.h"
#include "ArduinoTime.h"

static const char* MODULE_PREFIX = "ESPOTAUpdate";

// Debug
#define DEBUG_ESP_OTA_UPDATE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ESPOTAUpdate::ESPOTAUpdate(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig) 
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Extract info from config
    _otaDirectEnabled = (configGetLong("OTADirect", 1) != 0);
    
    // Restart pending
    _directUpdateRestartPending = false;
    _directUpdateRestartPendingStartMs = 0;

    // Direct update status
    _otaDirectUpdateHandle = -1;
    _otaDirectInProgress = false;

    // Timing of firmware update process
    _fwUpdateTimeStartedUs = 0;
    _fwUpdateBeginTimeUs = 0;
    _fwUpdateWriteTimeUs = 0;
    _fwUpdateBytes = 0;
    _fwUpdateLastRate = 0;
    _blockSizeLast = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::setup()
{
    LOG_I(MODULE_PREFIX, "setup otaDirect %s", _otaDirectEnabled ? "YES" : "NO");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::service()
{
    // Check if OTA direct restart is pending
    if (_directUpdateRestartPending && 
            Utils::isTimeout(millis(), _directUpdateRestartPendingStartMs, TIME_TO_WAIT_BEFORE_RESTART_MS))
    {
        esp_restart();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ESPOTAUpdate::getDebugJSON()
{
    // FW update timing
    uint32_t fwUpdateElapsedMs = (micros() - _fwUpdateTimeStartedUs) / 1000;
    if (_otaDirectInProgress)
        _fwUpdateLastRate = (fwUpdateElapsedMs != 0) ? 1000.0*_fwUpdateBytes/fwUpdateElapsedMs : 0;
    char tmpBuf[200];
    snprintf(tmpBuf, sizeof(tmpBuf)-1, R"({"Bps":%.1f,"stMs":%d,"bytes":%d,"wrPS":%.1f,"elapS":%.1f,"blk":%d})",
        _fwUpdateLastRate, 
        (uint32_t)(_fwUpdateBeginTimeUs / 1000),
        _fwUpdateBytes,
        _fwUpdateWriteTimeUs != 0 ? _fwUpdateBytes / (_fwUpdateWriteTimeUs / 1000000.0) : 0.0,
        fwUpdateElapsedMs / 1000.0,
        _blockSizeLast);
    return tmpBuf;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Firmware update start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ESPOTAUpdate::fileStreamStart(const char* fileName, size_t fileLen)
{
    // Check enabled
    if (!_otaDirectEnabled)
    {
        LOG_W(MODULE_PREFIX, "fileStreamStart OTA Direct Disabled");
        return false;
    }
    
    // Timing
    _fwUpdateTimeStartedUs = micros();
    _fwUpdateBeginTimeUs = 0;
    _fwUpdateWriteTimeUs = 0;
    _fwUpdateBytes = 0;
    _blockSizeLast = 0;

    // Get update partition
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    LOG_I(MODULE_PREFIX, "fileStreamStart fileName %s fileLen %d", fileName, fileLen);
#endif

    // const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    // Info
    LOG_I(MODULE_PREFIX, "fileStreamStart running partition type %d subtype %d (offset 0x%x)",
                running->type, running->subtype, running->address);

    LOG_I(MODULE_PREFIX, "fileStreamStart writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);

    // Start OTA update
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &_otaDirectUpdateHandle);

    // Timing
    _fwUpdateBeginTimeUs = micros() - _fwUpdateTimeStartedUs;

    // Check result
    if (err != ESP_OK) 
    {
#ifdef DEBUG_ESP_OTA_UPDATE
        // Debug
        LOG_E(MODULE_PREFIX, "fileStreamStart esp_ota_begin failed, error=%d", err);
        return false;
#endif
    }
    else
    {
        _otaDirectInProgress = true;
#ifdef DEBUG_ESP_OTA_UPDATE
        // Debug
        LOG_I(MODULE_PREFIX, "fileStreamStart esp_ota_begin succeeded");
#endif
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Firmware update block (handle a firmware data block)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ESPOTAUpdate::fileStreamDataBlock(FileStreamBlock& fileStreamBlock)
{
    // Get params
    const uint8_t *pBlock = fileStreamBlock.pBlock;
    size_t blockLen = fileStreamBlock.blockLen;

    // Check if not in progress and first block
    if (!_otaDirectInProgress && fileStreamBlock.firstBlock)
    {
        if (!fileStreamStart(fileStreamBlock.filename, 
                    fileStreamBlock.fileLenValid ? fileStreamBlock.fileLen : fileStreamBlock.contentLen))
            return false;
    }

    // Check if in progress
    if (_otaDirectInProgress)
    {
        uint64_t fwStart = micros();
        esp_err_t err = esp_ota_write(_otaDirectUpdateHandle, (const void *)pBlock, blockLen);
        _fwUpdateWriteTimeUs += micros() - fwStart;
        _fwUpdateBytes += blockLen;
        _blockSizeLast = blockLen;
        if (err != ESP_OK) 
        {
            _otaDirectInProgress = false;
            LOG_E(MODULE_PREFIX, "esp_ota_write failed! err=0x%x", err);
            return false;
        }
    }

    // Check if final
    if (fileStreamBlock.finalBlock)
    {
        if (!firmwareUpdateEnd())
            return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Firmware update end (flash and reboot)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ESPOTAUpdate::firmwareUpdateEnd()
{
    // Check valid
    if (_otaDirectInProgress)
    {
        // Finish OTA
        _otaDirectInProgress = false;
        if (esp_ota_end(_otaDirectUpdateHandle) != ESP_OK) 
        {
            LOG_E(MODULE_PREFIX, "esp_ota_end failed!");
            return false;
        }

        // Get update partition
        const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

        // Set boot partition
        esp_err_t err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) 
        {
            LOG_E(MODULE_PREFIX, "esp_ota_set_boot_partition failed! err=0x%x", err);
            return false;
        }

#ifdef DEBUG_ESP_OTA_UPDATE
        // Debug
        LOG_I(MODULE_PREFIX, "esp_ota_set_boot_partition ok ... reboot pending");
#endif

        // Schedule restart
        _directUpdateRestartPendingStartMs = millis();
        _directUpdateRestartPending = true;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Firmware update cancel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ESPOTAUpdate::fileStreamCancelEnd(bool isNormalEnd)
{
    if (_otaDirectInProgress)
    {
        _otaDirectInProgress = false;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)        
        return esp_ota_abort(_otaDirectUpdateHandle) == ESP_OK;
#else
        return true;
#endif
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle the API update
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::fwUpdateAPIPart(FileStreamBlock& fileStreamBlock)
{
    // LOG_I(MODULE_PREFIX, "fwUpdateAPIPart %d, %d, %d, %d", contentLen, index, len, finalBlock);

    // Update block
    fileStreamDataBlock(fileStreamBlock);
}

void ESPOTAUpdate::fwUpdateAPIFinal()
{
    // This may not get called as we may be rebooting if successful
    LOG_I(MODULE_PREFIX, "fwUpdateAPIFinal DONE");
    delay(100);
    firmwareUpdateEnd();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("espFwUpdate",
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&ESPOTAUpdate::apiESPFirmwareUpdateDone, this, 
                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                        "Update ESP32 firmware", 
                        "application/json", 
                        NULL,
                        RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                        NULL, 
                        NULL,
                        std::bind(&ESPOTAUpdate::apiESPFirmwarePart, this, 
                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

// ESP Firmware update
void ESPOTAUpdate::apiESPFirmwarePart(const String& reqStr, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo)
{
    // Handle with OTA update
#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    // LOG_I(MODULE_PREFIX, "apiESPFirmwarePart");
#endif
    fwUpdateAPIPart(fileStreamBlock);
}

void ESPOTAUpdate::apiESPFirmwareUpdateDone(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Handle with OTA update
#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    LOG_I(MODULE_PREFIX, "apiESPFirmwareDone");
#endif
    fwUpdateAPIFinal();
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}
