/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manager for SysMods (System Modules)
// All modules that are core to the system should be derived from SysModBase
// These modules are then serviced by this manager's service function
// They can be enabled/disabled and reconfigured in a consistent way
// Also modules can be referred to by name to allow more complex interaction
//
// Rob Dobson 2019
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SysManager.h>
#include "SysModBase.h"
#include <JSONParams.h>
#include <Logger.h>
#include <RestAPIEndpointManager.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Utils.h"
#include <RICUtils.h>
#include <ESPUtils.h>
#include <NetworkSystem.h>
#include <DebugGlobals.h>

// Log prefix
static const char *MODULE_PREFIX = "SysMan";

// #define ONLY_ONE_MODULE_PER_SERVICE_LOOP 1

// Warn
#define WARN_ON_SYSMOD_SLOW_SERVICE

// Debug supervisor step (for hangup detection within a service call)
// Uses global logger variables - see logger.h
#define DEBUG_GLOB_SYSMAN 0
#define INCLUDE_PROTOCOL_FILE_UPLOAD_IN_STATS

// Debug
// #define DEBUG_LIST_SYSMODS
// #define DEBUG_SYSMOD_WITH_GLOBAL_VALUE
// #define DEBUG_SEND_CMD_JSON_PERF

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysManager::SysManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig)
{
    // Store mutable config
    _pMutableConfig = pMutableConfig;

    // No endpoints yet
    _pRestAPIEndpointManager = nullptr;
    _pProtocolEndpointManager = nullptr;

    // Register this manager to all objects derived from SysModBase
    SysModBase::setSysManager(this);

    // Module name
    _moduleName = pModuleName;

    // Status change callback
    _statsCB = nullptr;

    // Supervisor vector needs an update
    _supervisorDirty = false;
    _serviceLoopCurModIdx = 0;

    // Slow SysMod threshold
    _slowSysModThresholdUs = _sysModManConfig.getLong("slowSysModMs", SLOW_SYS_MOD_THRESHOLD_MS_DEFAULT) * 1000;

    // Stress test
    _stressTestLoopDelayMs = 0;
    _stressTestLoopSkipCount = 0;
    _stressTestCurSkipCount = 0;

    // Extract info from config
    _sysModManConfig = pGlobalConfig ? 
                pGlobalConfig->getString(_moduleName.c_str(), "{}") :
                defaultConfig.getString(_moduleName.c_str(), "{}");

    // Extract system name from config
    _systemName = defaultConfig.getString("SystemName", "RIC");
    _systemVersion = defaultConfig.getString("SystemVersion", "0.0.0");

    // Monitoring period and monitoring timer
    _monitorPeriodMs = _sysModManConfig.getLong("monitorPeriodMs", 10000);
    _monitorTimerMs = millis();
    _sysModManConfig.getArrayElems("reportList", _monitorReportList);
    _monitorTimerStarted = false;

    // System restart flag
    _systemRestartPending = false;
    _systemRestartMs = millis();

    // System friendly name
    _defaultFriendlyName = defaultConfig.getString("DefaultName", "");
    _defaultFriendlyNameIsSet = defaultConfig.getLong("DefaultNameIsSet", 0);

    // System unique string - use BT MAC address
    _systemUniqueString = getSystemMACAddressStr(ESP_MAC_BT, "");

    // Get mutable config info
    _friendlyNameIsSet = false;
    if (_pMutableConfig)
    {
        _friendlyNameStored = _pMutableConfig->getString("friendlyName", "");
        _friendlyNameIsSet = _pMutableConfig->getLong("nameSet", 0);
        _ricSerialNoStoredStr = _pMutableConfig->getString("serialNo", "");

        // Setup network system hostname
        if (_friendlyNameIsSet)
            networkSystem.setHostname(_friendlyNameStored.c_str());
    }

    // File stream activity
    _isSystemMainFWUpdate = false;
    _isSystemFileTransferring = false;
    _isSystemStreaming = false;

    // Debug
    LOG_I(MODULE_PREFIX, "friendlyNameStored %s, friendlyNameIsSet %s, defaultFriendlyName %s defaultFriendlyNameIsSet %d",
                 _friendlyNameStored.c_str(), _friendlyNameIsSet ? "Y" : "N", _defaultFriendlyName.c_str(), _defaultFriendlyNameIsSet);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::setup()
{
    // Clear status change callbacks for sysmods (they are added again in postSetup)
    clearAllStatusChangeCBs();

    // Add our own REST endpoints
    if (_pRestAPIEndpointManager)
    {
        _pRestAPIEndpointManager->addEndpoint("reset", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiReset, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                "Restart program");
        _pRestAPIEndpointManager->addEndpoint("v", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetVersion, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                "Get version info");
        _pRestAPIEndpointManager->addEndpoint("sysmodinfo", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetSysModInfo, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                "Get sysmod info");
        _pRestAPIEndpointManager->addEndpoint("sysmoddebug", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetSysModDebug, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                "Get sysmod debug");
        _pRestAPIEndpointManager->addEndpoint("friendlyname", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiFriendlyName, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "Friendly name for system");
        _pRestAPIEndpointManager->addEndpoint("serialno", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiSerialNumber, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "Serial number");
        _pRestAPIEndpointManager->addEndpoint("hwrevno", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiHwRevisionNumber, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "HW revision number");
        _pRestAPIEndpointManager->addEndpoint("testsetloopdelay", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiTestSetLoopDelay, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "Set a loop delay to test resilience, e.g. ?delayMs=10&skipCount=1, applies 10ms delay to alternate loops");
        _pRestAPIEndpointManager->addEndpoint("sysman", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiSysManSettings, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                "Set SysMan settings, e.g. sysman?interval=2&rxBuf=10240");
    }

    // Short delay here to allow logging output to complete as some hardware configurations
    // require changes to serial uarts and this disturbs the logging flow
    delay(100);

    // Now call setup on system modules
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
            pSysMod->setup();
    }

    // Add endpoints for system modules
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
        {
            if (_pRestAPIEndpointManager)
                pSysMod->addRestAPIEndpoints(*_pRestAPIEndpointManager);
            if (_pProtocolEndpointManager)
                pSysMod->addProtocolEndpoints(*_pProtocolEndpointManager);
        }            
    }

    // Post-setup - called after setup of all sysMods complete
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
            pSysMod->postSetup();
    }

    // Check if WiFi to be paused when BLE connected
    bool pauseWiFiForBLEConn = _sysModManConfig.getString("pauseWiFiforBLE", 0);
    LOG_I(MODULE_PREFIX, "pauseWiFiForBLEConn %s", pauseWiFiForBLEConn ? "YES" : "NO");
    if (pauseWiFiForBLEConn)
    {
        // Hook status change on BLE
        setStatusChangeCB("BLEMan", 
                std::bind(&SysManager::statusChangeBLEConnCB, this, std::placeholders::_1, std::placeholders::_2));
    }

#ifdef DEBUG_LIST_SYSMODS
    uint32_t sysModIdx = 0;
    for (SysModBase* pSysMod : _sysModuleList)
    {
        LOG_I(MODULE_PREFIX, "SysMod %d: %s", sysModIdx++, 
                pSysMod ? pSysMod->modName() : "UNKNOWN");
            
    }
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::service()
{
    // Check if supervisory info is dirty
    if (_supervisorDirty)
    {
        // Ideally this operation should be atomic but we assume currently system modules
        // are only added before the main program loop is entered
        supervisorSetup();
        _supervisorDirty = false;
    }
       
    // Service monitor periodically records timing
    if (_monitorTimerStarted)
    {
        // Check if monitor period is up
        if (Utils::isTimeout(millis(), _monitorTimerMs, _monitorPeriodMs))
        {
            // Calculate supervisory stats
            _supervisorStats.calculate();

            // Show stats
            statsShow();

            // Clear stats for start of next monitor period
            _supervisorStats.clear();

            // Wait until next period
            _monitorTimerMs = millis();
        }
    }
    else
    {
        // Start monitoring
        _supervisorStats.clear();
        _monitorTimerMs = millis();
        _monitorTimerStarted = true;
    }

    // Check the index into list of sys-mods to service is valid
    uint32_t numSysMods = _sysModServiceVector.size();
    if (numSysMods == 0)
        return;
    if (_serviceLoopCurModIdx >= numSysMods)
        _serviceLoopCurModIdx = 0;

    // Monitor how long it takes to go around loop
    _supervisorStats.outerLoopStarted();

#ifndef ONLY_ONE_MODULE_PER_SERVICE_LOOP
    for (_serviceLoopCurModIdx = 0; _serviceLoopCurModIdx < numSysMods; _serviceLoopCurModIdx++)
    {
#endif

    if (_sysModServiceVector[_serviceLoopCurModIdx])
    {
#ifdef DEBUG_SYSMOD_WITH_GLOBAL_VALUE
        DEBUG_GLOB_VAR_NAME(DEBUG_GLOB_SYSMAN) = _serviceLoopCurModIdx;
#endif
#ifdef WARN_ON_SYSMOD_SLOW_SERVICE
        uint64_t sysModExecStartUs = micros();
#endif

        // Service SysMod
        _supervisorStats.execStarted(_serviceLoopCurModIdx);
        _sysModServiceVector[_serviceLoopCurModIdx]->service();
        _supervisorStats.execEnded(_serviceLoopCurModIdx);

#ifdef WARN_ON_SYSMOD_SLOW_SERVICE
        uint64_t sysModServiceUs = micros() - sysModExecStartUs;
        if (sysModServiceUs > _slowSysModThresholdUs)
        {
            LOG_W(MODULE_PREFIX, "service sysMod %s SLOW took %lldms", _sysModServiceVector[_serviceLoopCurModIdx]->modName(), sysModServiceUs/1000);
        }
#endif
    }

#ifndef ONLY_ONE_MODULE_PER_SERVICE_LOOP
    }
#endif

    // Debug
    // LOG_D(MODULE_PREFIX, "Service module %s", sysModInfo._pSysMod->modName());

    // Next SysMod
    _serviceLoopCurModIdx++;

    // Check system restart pending
    if (_systemRestartPending)
    {
        if (Utils::isTimeout(millis(), _systemRestartMs, SYSTEM_RESTART_DELAY_MS))
        {
            _systemRestartPending = false;
            esp_restart();
        }
    }

    // End loop
    _supervisorStats.outerLoopEnded();

    // Stress testing
    if (_stressTestLoopDelayMs > 0)
    {
        if (_stressTestCurSkipCount >= _stressTestLoopSkipCount)
        {
            delay(_stressTestLoopDelayMs);
            _stressTestCurSkipCount = 0;
        }
        else
        {
            _stressTestCurSkipCount++;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage SysMod List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::add(SysModBase* pSysMod)
{
    // Avoid adding null pointers
    if (!pSysMod)
        return;

    // Add to module list
    _sysModuleList.push_back(pSysMod);

    // Supervisor info now dirty
    _supervisorDirty = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Supervisor Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::supervisorSetup()
{
    // Reset iterator to start of list
    _serviceLoopCurModIdx = 0;

    // Clear stats
    _supervisorStats.clear();

    // Clear and reserve sysmods from service vector
    _sysModServiceVector.clear();
    _sysModServiceVector.reserve(_sysModuleList.size());

    // Add modules to list and initialise stats
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
        {
            _sysModServiceVector.push_back(pSysMod);
            _supervisorStats.add(pSysMod->modName());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add status change callback on a SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::setStatusChangeCB(const char* sysModName, SysMod_statusChangeCB statusChangeCB)
{
    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            // Debug
            LOG_I(MODULE_PREFIX, "setStatusChangeCB sysMod %s cbValid %d sysModFound OK", sysModName, statusChangeCB != nullptr);
            return pSysMod->setStatusChangeCB(statusChangeCB);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear all status change callbacks on all sysmods
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::clearAllStatusChangeCBs()
{
    // Debug
    // LOG_I(MODULE_PREFIX, "clearAllStatusChangeCBs");
    // Go through the sysmod list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        return pSysMod->clearStatusChangeCBs();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status from SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String SysManager::getStatusJSON(const char* sysModName)
{
    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            return pSysMod->getStatusJSON();
        }
    }
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get debugStr from SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String SysManager::getDebugJSON(const char* sysModName)
{
    // Check if it is the SysManager's own stats that are wanted
    if (strcasecmp(sysModName, "SysMan") == 0)
        return _supervisorStats.getSummaryString();

    // Check for global debug values
    if (strcasecmp(sysModName, "Globs") == 0)
    {
        return DebugGlobals::getDebugJson(false);
    }

    // Check for stats callback
    if (strcasecmp(sysModName, "StatsCB") == 0)
    {
        if (_statsCB)
            return _statsCB();
        return "";
    }

    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            return pSysMod->getDebugJSON();
        }
    }
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send command to SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::sendCmdJSON(const char* sysModName, const char* cmdJSON)
{
    // See if the sysmod is in the list
#ifdef DEBUG_SEND_CMD_JSON_PERF
    uint64_t startUs = micros();
#endif
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
#ifdef DEBUG_SEND_CMD_JSON_PERF
            uint64_t foundSysModUs = micros();
#endif

            pSysMod->receiveCmdJSON(cmdJSON);

#ifdef DEBUG_SEND_CMD_JSON_PERF
            LOG_I(MODULE_PREFIX, "getHWElemByName %s found in %lldus exec time %lldus", 
                    sysModName, foundSysModUs - startUs, micros() - foundSysModUs);
#endif
            return;
        }
    }
#ifdef DEBUG_SEND_CMD_JSON_PERF
    LOG_I(MODULE_PREFIX, "getHWElemByName %s NOT found in %lldus", sysModName, micros() - startUs);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message-generator callback to SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::sendMsgGenCB(const char* sysModName, const char* msgGenID, SysMod_publishMsgGenFn msgGenCB, SysMod_stateDetectCB stateDetectCB)
{
    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            pSysMod->receiveMsgGenCB(msgGenID, msgGenCB, stateDetectCB);
            return;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiReset(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Register that a restart is required but don't restart immediately
    // as the acknowledgement would not get through
    systemRestart();

    // Result
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

void SysManager::apiGetVersion(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    char versionJson[225];
    snprintf(versionJson, sizeof(versionJson),
             R"({"req":"%s","rslt":"ok","SystemName":"%s","SystemVersion":"%s","SerialNo":"%s",)"
             R"("MAC":"%s","RicHwRevNo":%d})",
             reqStr.c_str(), 
             _systemName.c_str(), 
             _systemVersion.c_str(), 
             _ricSerialNoStoredStr.c_str(),
             _systemUniqueString.c_str(), 
             getRICRevision());
    respStr = versionJson;
}

void SysManager::apiGetSysModInfo(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    respStr = getStatusJSON(sysModName.c_str());
}

void SysManager::apiGetSysModDebug(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String debugStr = "\"debug\":" + getDebugJSON(sysModName.c_str());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, debugStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Friendly name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiFriendlyName(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Check if we're setting
    if (RestAPIEndpointManager::getNumArgs(reqStr.c_str()) > 1)
    {
        // Set name
        bool nameIsSet = false;
        String friendlyName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
        friendlyName.trim();
        if (friendlyName.length() > MAX_FRIENDLY_NAME_LENGTH)
        {
            Utils::setJsonErrorResult(reqStr.c_str(), respStr, "nameTooLong");
            return;
        }
        else if (friendlyName.length() == 0)
        {
            // Return to default friendly name
            LOG_I(MODULE_PREFIX, "apiFriendlyName set friendlyName blank -> default");
        }
        else
        {
            LOG_I(MODULE_PREFIX, "apiFriendlyName set friendlyName %s", friendlyName.c_str());
            nameIsSet = true;
        }
        _friendlyNameStored = friendlyName;
        _friendlyNameIsSet = nameIsSet;

        // Setup network system hostname
        if (_friendlyNameIsSet)
            networkSystem.setHostname(_friendlyNameStored.c_str());

        // Store the new name (even if it is blank)
        String jsonConfig = getMutableConfigJson();
        if (_pMutableConfig)
        {
            _pMutableConfig->writeConfig(jsonConfig);
        }
    }

    // Get current 
    String friendlyName = getFriendlyName();
    LOG_I(MODULE_PREFIX, "apiFriendlyName -> %s, nameIsSet %s", friendlyName.c_str(), _friendlyNameIsSet ? "Y" : "N");

    // Create response JSON
    char JsonOut[MAX_FRIENDLY_NAME_LENGTH + 70];
    snprintf(JsonOut, sizeof(JsonOut), R"("friendlyName":"%s","friendlyNameIsSet":%d)", 
                friendlyName.c_str(), _friendlyNameIsSet);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, JsonOut);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serial number
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiSerialNumber(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Check if we're setting
    if (RestAPIEndpointManager::getNumArgs(reqStr.c_str()) > 1)
    {
        // Get serial number to set
        String serialNoHexStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
        uint8_t serialNumBuf[RIC_SERIAL_NUMBER_BYTES];
        if (Utils::getBytesFromHexStr(serialNoHexStr.c_str(), serialNumBuf, RIC_SERIAL_NUMBER_BYTES) != RIC_SERIAL_NUMBER_BYTES)
        {
            Utils::setJsonErrorResult(reqStr.c_str(), respStr, "SNNot16Byt");
            return;
        }

        // Validate magic string
        String magicString;
        if (RestAPIEndpointManager::getNumArgs(reqStr.c_str()) > 2)
        {
            magicString = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
            if (!magicString.equals(RIC_SERIAL_SET_MAGIC_STR))
            {
                Utils::setJsonErrorResult(reqStr.c_str(), respStr, "SNNeedsMagic");
                return;
            }
        }

        // Get formatted serial no
        Utils::getHexStrFromBytes(serialNumBuf, RIC_SERIAL_NUMBER_BYTES, _ricSerialNoStoredStr);

        // Store the serial no
        String jsonConfig = getMutableConfigJson();
        if (_pMutableConfig)
        {
            _pMutableConfig->writeConfig(jsonConfig);
        }
    }

    // Create response JSON
    char JsonOut[MAX_FRIENDLY_NAME_LENGTH + 70];
    snprintf(JsonOut, sizeof(JsonOut), R"("SerialNo":"%s")", _ricSerialNoStoredStr.c_str());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, JsonOut);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HW revision number
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiHwRevisionNumber(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Create response JSON
    char jsonOut[30];
    snprintf(jsonOut, sizeof(jsonOut), R"("RicHwRevNo":%d)", getRICRevision());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, jsonOut);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test function to set loop delay
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiTestSetLoopDelay(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    JSONParams nameValueParamsJson = RdJson::getJSONFromNVPairs(nameValues, true);

    // Extract values
    _stressTestLoopDelayMs = nameValueParamsJson.getLong("delayMs", 0);
    _stressTestLoopSkipCount = nameValueParamsJson.getLong("skipCount", 0);
    _stressTestCurSkipCount = 0;

    // Debug
    LOG_I(MODULE_PREFIX, "apiTestSetLoopDelay delay %dms skip %d loops", _stressTestLoopDelayMs, _stressTestLoopSkipCount);

    // Create response JSON
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup SysMan diagnostics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiSysManSettings(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    JSONParams nameValueParamsJson = RdJson::getJSONFromNVPairs(nameValues, true);

    // Extract log output interval
    _monitorPeriodMs = nameValueParamsJson.getDouble("interval", _monitorPeriodMs/1000) * 1000;
    if (_monitorPeriodMs < 1000)
        _monitorPeriodMs = 1000;

    // Extract list of modules to show information on
    std::vector<String> reportList;
    if (nameValueParamsJson.getArrayElems("report", reportList))
    {
        _monitorReportList = reportList;
        // for (const String& s : reportList)
        //     LOG_I(MODULE_PREFIX, "apiSysManSettings reportitem %s", s.c_str());
    }

    // Check for baud-rate change
    int baudRate = nameValueParamsJson.getLong("baudRate", -1);
    String debugStr;
    if (baudRate >= 0)
    {
        // Configure the serial console
        String cmdJson = "{\"cmd\":\"set\",\"baudRate\":" + String(baudRate) + "}";
        sendCmdJSON("SerialConsole", cmdJson.c_str());
        debugStr = " baudRate " + String(baudRate);
    }

    // Check for buffer-size changes
    int rxBufSize = nameValueParamsJson.getLong("rxBuf", -1);
    int txBufSize = nameValueParamsJson.getLong("txBuf", -1);
    if ((rxBufSize >= 0) || (txBufSize >= 0))
    {
        // Configure the serial console
        String cmdJson = "{\"cmd\":\"set\"";
        if (rxBufSize >= 0)
        {
            cmdJson += ",\"rxBuf\":" + String(rxBufSize);
            debugStr = " rxBufSize " + String(rxBufSize);
        }
        if (txBufSize >= 0)
        {
            cmdJson += ",\"txBuf\":" + String(txBufSize);
            debugStr = " txBufSize " + String(txBufSize);
        }
        cmdJson += "}";
        sendCmdJSON("SerialConsole", cmdJson.c_str());
    }

    // Debug
    LOG_I(MODULE_PREFIX, "apiSysManSettings report interval %.1f secs reportList %s%s", 
                _monitorPeriodMs/1000.0, nameValueParamsJson.getString("report","").c_str(),
                debugStr.c_str());

    // Create response JSON
    String reqStrWithoutQuotes = reqStr;
    reqStrWithoutQuotes.replace("\"","");
    Utils::setJsonBoolResult(reqStrWithoutQuotes.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get mutable config JSON string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String SysManager::getMutableConfigJson()
{
    char jsonConfig[MAX_FRIENDLY_NAME_LENGTH + RIC_SERIAL_NUMBER_BYTES*2 + 40];
    snprintf(jsonConfig, sizeof(jsonConfig), 
            R"({"friendlyName":"%s","nameSet":%d,"serialNo":"%s"})", 
                _friendlyNameStored.c_str(), _friendlyNameIsSet, _ricSerialNoStoredStr.c_str());
    return jsonConfig;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stats show
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::statsShow()
{
    // Generate stats
    char statsStr[200];
    snprintf(statsStr, sizeof(statsStr), R"({"n":"%s","v":"%s","r":%d,"hpInt":%d,"hpMin":%d,"hpAll":%d)", 
                _systemName.c_str(),
                _systemVersion.c_str(),
                getRICRevision(),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // Stats string
    String statsOut = statsStr;

    // Add stats
    for (String& srcStr : _monitorReportList)
    {
        String modStr = getDebugJSON(srcStr.c_str());
        if (modStr.length() > 2)
            statsOut += ",\"" + srcStr + "\":" + modStr;
    }

    // Terminate JSON
    statsOut += "}";

    // Report stats
    LOG_I(MODULE_PREFIX, "%s", statsOut.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Friendly name helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String SysManager::getFriendlyName()
{
    // Handle default naming
    String friendlyName = _friendlyNameStored;
    if (friendlyName.length() == 0)
    {
        friendlyName = _defaultFriendlyName;
        LOG_I(MODULE_PREFIX, "getFriendlyName default %s uniqueStr %s", _defaultFriendlyName.c_str(), _systemUniqueString.c_str());
        if (_systemUniqueString.length() >= 6)
            friendlyName += "_" + _systemUniqueString.substring(_systemUniqueString.length()-6);
    }
    return friendlyName;
}

bool SysManager::getFriendlyNameIsSet()
{
    if (_pMutableConfig)
        return _pMutableConfig->getLong("nameSet", 0);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status change CB on BLE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::statusChangeBLEConnCB(const String& sysModName, bool changeToOnline)
{
    // Check if WiFi should be paused
    LOG_I(MODULE_PREFIX, "BLE connection change isConn %s", changeToOnline ? "YES" : "NO");
    bool pauseWiFiForBLEConn = _sysModManConfig.getString("pauseWiFiforBLE", 0);
    if (pauseWiFiForBLEConn)
    {
        networkSystem.pauseWiFi(changeToOnline);
    }
}

