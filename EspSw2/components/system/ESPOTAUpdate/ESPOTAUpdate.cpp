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

static const char* MODULE_PREFIX = "ESPOTAUpdate";

// Debug
#define DEBUG_ESP_OTA_UPDATE 1

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
    _fwUpdateStartTimeUs = 0;
    _fwUpdateWriteTimeUs = 0;
    _fwUpdateBytes = 0;
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

String ESPOTAUpdate::getDebugStr()
{
    // FW update timing
    char tmpBuf[200];
    snprintf(tmpBuf, sizeof(tmpBuf)-1, "OTAStart %d ms OTAWrite %d ms Bytes %d Rate %.1fBytes/s",
        (uint32_t)(_fwUpdateStartTimeUs / 1000),
        (uint32_t)(_fwUpdateWriteTimeUs / 1000),
        _fwUpdateBytes,
        _fwUpdateBytes / (_fwUpdateWriteTimeUs / 1000000.0));
    return tmpBuf;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle the update
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ESPOTAUpdate::firmwareUpdateStart(const char* fileName, size_t fileLen)
{
    // Check enabled
    if (!_otaDirectEnabled)
    {
        LOG_W(MODULE_PREFIX, "firmwareUpdateStart OTA Direct Disabled");
        return false;
    }
    
    // Timing
    uint64_t fwStart = micros();
    _fwUpdateStartTimeUs = 0;
    _fwUpdateWriteTimeUs = 0;
    _fwUpdateBytes = 0;

    // Get update partition
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    LOG_I(MODULE_PREFIX, "firmwareUpdateStart fileName %s fileLen %d", fileName, fileLen);
#endif

    // const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    // Info
    LOG_I(MODULE_PREFIX, "firmwareUpdateStart running partition type %d subtype %d (offset 0x%x)",
                running->type, running->subtype, running->address);

    LOG_I(MODULE_PREFIX, "firmwareUpdateStart writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);

    // Start OTA update
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &_otaDirectUpdateHandle);

    // Timing
    _fwUpdateStartTimeUs = micros() - fwStart;

    // Check result
    if (err != ESP_OK) 
    {
#ifdef DEBUG_ESP_OTA_UPDATE
        // Debug
        LOG_E(MODULE_PREFIX, "firmwareUpdateStart esp_ota_begin failed, error=%d", err);
        return false;
#endif
    }
    else
    {
        _otaDirectInProgress = true;
#ifdef DEBUG_ESP_OTA_UPDATE
        // Debug
        LOG_I(MODULE_PREFIX, "firmwareUpdateStart esp_ota_begin succeeded");
#endif
    }
    return true;
}

bool ESPOTAUpdate::firmwareUpdateBlock(uint32_t filePos, const uint8_t *pBlock, size_t blockLen)
{
    // Check if in progress
    if (_otaDirectInProgress)
    {
        uint64_t fwStart = micros();
        esp_err_t err = esp_ota_write(_otaDirectUpdateHandle, (const void *)pBlock, blockLen);
        _fwUpdateWriteTimeUs += micros() - fwStart;
        _fwUpdateBytes += blockLen;
        if (err != ESP_OK) 
        {
            _otaDirectInProgress = false;
            LOG_E(MODULE_PREFIX, "esp_ota_write failed! err=0x%x", err);
            return false;
        }
    }
    return true;
}

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
// Handle the API update
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::fwUpdateAPIPart(FileBlockInfo& fileBlockInfo)
{
    // LOG_I(MODULE_PREFIX, "fwUpdateAPIPart %d, %d, %d, %d", contentLen, index, len, finalBlock);

    // Start if at the beginning
    if (fileBlockInfo.filePos == 0)
        if (!firmwareUpdateStart(fileBlockInfo.filename, 
                    fileBlockInfo.fileLenValid ? fileBlockInfo.fileLen : fileBlockInfo.contentLen))
            return;

    // Update block
    if (!firmwareUpdateBlock(fileBlockInfo.filePos, fileBlockInfo.pBlock, fileBlockInfo.blockLen))
        return;

    // Check if final block
    if (fileBlockInfo.finalBlock)
        if (!firmwareUpdateEnd())
            return;

    // All done
    return;
}

void ESPOTAUpdate::fwUpdateAPIFinal()
{
    // This may not get called as we may be rebooting if successful
    LOG_I(MODULE_PREFIX, "fwUpdateAPIFinal DONE");
    delay(100);
    if (_otaDirectInProgress)
    {
        esp_ota_end(_otaDirectUpdateHandle);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ESPOTAUpdate::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("espfwupdate",
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&ESPOTAUpdate::apiESPFirmwareUpdateDone, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Update ESP32 firmware", 
                        "application/json", 
                        NULL,
                        RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                        NULL, 
                        NULL,
                        std::bind(&ESPOTAUpdate::apiESPFirmwarePart, this, 
                                std::placeholders::_1, std::placeholders::_2));
}

// ESP Firmware update
void ESPOTAUpdate::apiESPFirmwarePart(const String& reqStr, FileBlockInfo& fileBlockInfo)
{
    // Handle with OTA update
#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    // LOG_I(MODULE_PREFIX, "apiESPFirmwarePart");
#endif
    fwUpdateAPIPart(fileBlockInfo);
}

void ESPOTAUpdate::apiESPFirmwareUpdateDone(const String &reqStr, String &respStr)
{
    // Handle with OTA update
#ifdef DEBUG_ESP_OTA_UPDATE
    // Debug
    LOG_I(MODULE_PREFIX, "apiESPFirmwareDone");
#endif
    fwUpdateAPIFinal();
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}
