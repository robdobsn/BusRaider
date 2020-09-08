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
#include <ConfigBase.h>
#include <Logger.h>
#include <RestAPIEndpointManager.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Utils.h"
#include <ESPUtils.h>
#include <NetworkSystem.h>
#include <ProtocolRICSerial.h>
#include <ProtocolRICFrame.h>
#include <RICRESTMsg.h>
#include <FileSystem.h>

// Log prefix
static const char *MODULE_PREFIX = "SysMan";

// #define ONLY_ONE_MODULE_PER_SERVICE_LOOP 1

// Debug
// #define DEBUG_RICREST_MESSAGES
// #define DEBUG_ENDPOINT_MESSAGES
#define DEBUG_SHOW_FILE_UPLOAD_STATS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysManager::SysManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pMutableConfig)
{
    // Store mutable config
    _pMutableConfig = pMutableConfig;

    // No endpoints yet
    _pRestAPIEndpointManager = NULL;
    _pProtocolEndpointManager = NULL;

    // Register this manager to all objects derived from SysModBase
    SysModBase::setSysManager(this);

    // Module name
    _moduleName = pModuleName;

    // Status change callback
    _statsCB = NULL;

    // Supervisor vector needs update
    _supervisorDirty = false;
    _supervisorCurModIdx = 0;

    // Extract info from config
    _sysModManConfig = defaultConfig.getString(_moduleName.c_str(), "{}");

    // Extract system name from config
    _systemName = defaultConfig.getString("SystemName", "RBot");
    _systemVersion = defaultConfig.getString("SystemVersion", "0.0.0");

    // Monitoring period and monitoring timer
    _monitorPeriodMs = _sysModManConfig.getLong("monitorPeriodMs", 10000);
    _monitorTimerMs = millis();
    _monitorTimerStarted = false;

    // System restart flag
    _systemRestartPending = false;
    _systemRestartMs = millis();

    // System friendly name
    _defaultFriendlyName = defaultConfig.getString("DefaultName", "");
    _defaultFriendlyNameIsSet = defaultConfig.getLong("DefaultNameIsSet", 0);

    // System unique string - use BT MAC address
    _systemUniqueString = getSystemMACAddressStr(ESP_MAC_BT, "");

    // Firmware updater
    _pFirmwareUpdateSysMod = NULL;

    // Get mutable config info
    _friendlyNameIsSet = false;
    if (_pMutableConfig)
    {
        _friendlyNameStored = _pMutableConfig->getString("friendlyName", "");
        _friendlyNameIsSet = _pMutableConfig->getLong("nameSet", 0);
        _ricSerialNoStoredStr = _pMutableConfig->getString("serialNo", "");
    }

    // Debug
    LOG_I(MODULE_PREFIX, "friendlyNameStored %s, friendlyNameIsSet %s, defaultFriendlyName %s defaultFriendlyNameIsSet %d",
                 _friendlyNameStored.c_str(), _friendlyNameIsSet ? "Y" : "N", _defaultFriendlyName.c_str(), _defaultFriendlyNameIsSet);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::setup()
{
    // Clear firmware update SysMod
    _pFirmwareUpdateSysMod = NULL;

    // Clear status change callbacks for sysmods (they are added again in postSetup)
    clearAllStatusChangeCBs();

    // Add our own REST endpoints
    if (_pRestAPIEndpointManager)
    {
        _pRestAPIEndpointManager->addEndpoint("reset", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiReset, this, std::placeholders::_1, std::placeholders::_2), 
                "Restart program");
        _pRestAPIEndpointManager->addEndpoint("v", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetVersion, this, std::placeholders::_1, std::placeholders::_2), 
                "Get version info");
        _pRestAPIEndpointManager->addEndpoint("sysmodinfo", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetSysModInfo, this, std::placeholders::_1, std::placeholders::_2), 
                "Get sysmod info");
        _pRestAPIEndpointManager->addEndpoint("sysmoddebug", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&SysManager::apiGetSysModDebug, this, std::placeholders::_1, std::placeholders::_2), 
                "Get sysmod debug");
        _pRestAPIEndpointManager->addEndpoint("friendlyname", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiFriendlyName, this, std::placeholders::_1, std::placeholders::_2),
                "Friendly name for system");
        _pRestAPIEndpointManager->addEndpoint("serialno", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiSerialNumber, this, std::placeholders::_1, std::placeholders::_2),
                "Serial number");
    }

    // Add support for RICSerial
    String ricSerialConfig = _sysModManConfig.getString("RICSerial", "{}");
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints - adding RICSerial config %s", ricSerialConfig.c_str());
    ProtocolDef ricSerialProtocolDef = { "RICSerial", ProtocolRICSerial::createInstance, ricSerialConfig, 
                        std::bind(&SysManager::processEndpointMsg, this, std::placeholders::_1) };
    if (_pProtocolEndpointManager)
        _pProtocolEndpointManager->addProtocol(ricSerialProtocolDef);

    // Add support for RICFrame
    String ricFrameConfig = _sysModManConfig.getString("RICFrame", "{}");
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints - adding RICFrame config %s", ricFrameConfig.c_str());
    ProtocolDef ricFrameProtocolDef = { "RICFrame", ProtocolRICFrame::createInstance, ricFrameConfig, 
                        std::bind(&SysManager::processEndpointMsg, this, std::placeholders::_1) };
    if (_pProtocolEndpointManager)
        _pProtocolEndpointManager->addProtocol(ricFrameProtocolDef);

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
        setStatusChangeCB("BLEManager", std::bind(&SysManager::statusChangeBLEConnCB, this));
    }

    // Keep track of the firmware updater
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
        {
            if (pSysMod->isFirmwareUpdateModule())
            {
                _pFirmwareUpdateSysMod = pSysMod;
                break;
            }
        }
    }

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
            // Record current stats
            statsUpdate();

            // Show stats
            statsShow();

            // Clear stats for start of next monitor period
            statsClear();

            // Wait until next period
            _monitorTimerMs = millis();
        }
    }
    else
    {
        // Start monitoring
        statsClear();
        _monitorTimerMs = millis();
        _monitorTimerStarted = true;
    }

    // Check empty list
    uint32_t numSysMods = _supervisorSysModInfoVector.size();
    if (numSysMods == 0)
        return;
    if (_supervisorCurModIdx >= numSysMods)
        _supervisorCurModIdx = 0;

    // Monitor how long it takes to go around loop
    _serviceLoopInfo.startLoop();

#ifndef ONLY_ONE_MODULE_PER_SERVICE_LOOP
    for (_supervisorCurModIdx = 0; _supervisorCurModIdx < _supervisorSysModInfoVector.size(); _supervisorCurModIdx++)
    {
#endif

    // Service one module on each loop
    SysModInfo& sysModInfo = _supervisorSysModInfoVector[_supervisorCurModIdx];
    if (sysModInfo._pSysMod)
    {
        // Start SysMod timer
        sysModInfo._sysModStats.started();
        sysModInfo._pSysMod->service();
        sysModInfo._sysModStats.ended();
    }

#ifndef ONLY_ONE_MODULE_PER_SERVICE_LOOP
    }
#endif

    // Debug
    // LOG_D(MODULE_PREFIX, "%sService module %s", "SysManager", sysModInfo._pSysMod->modName());

    // Next SysMod
    _supervisorCurModIdx++;

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
    _serviceLoopInfo.endLoop();

    // Service upload
    serviceFileUpload();
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
    _supervisorCurModIdx = 0;

    // Remove all supervisor info
    _supervisorSysModInfoVector.clear();

    // Reserve storage for the list
    _supervisorSysModInfoVector.reserve(_sysModuleList.size());

    // Add modules to list and initialise stats
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
        {
            _supervisorSysModInfoVector.push_back(SysModInfo(pSysMod));
        }
    }

    // Clear info about overall service timing
    _serviceLoopInfo.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Loop Timing - Start and End Loop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::ServiceLoopInfo::startLoop()
{
    _lastLoopStartMicros = micros();
}

void SysManager::ServiceLoopInfo::endLoop()
{
    int64_t loopTime = Utils::timeElapsed(micros(), _lastLoopStartMicros);
    _loopTimeAvgSum += loopTime;
    _loopTimeAvgCount++;
    if (_loopTimeMin > loopTime)
        _loopTimeMin = loopTime;
    if (_loopTimeMax < loopTime)
        _loopTimeMax = loopTime;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Loop Timing - Gen Stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::statsUpdate()
{
    // Overall loop times
    _supervisorStats._totalLoops = _serviceLoopInfo._loopTimeAvgCount;
    _supervisorStats._loopTimeMinUs = _serviceLoopInfo._loopTimeMin;
    _supervisorStats._loopTimeMaxUs = _serviceLoopInfo._loopTimeMax;
    _supervisorStats._loopTimeAvgUs = 0;
    if (_serviceLoopInfo._loopTimeAvgCount > 0)
        _supervisorStats._loopTimeAvgUs = (1.0 * _serviceLoopInfo._loopTimeAvgSum) / _serviceLoopInfo._loopTimeAvgCount;

    // Slowest SysMod
    _supervisorStats._slowestSysModIdx = 0;
    for (std::size_t i = 0; i < _supervisorSysModInfoVector.size(); i++)
    {
        if (_supervisorSysModInfoVector[i]._sysModStats._execMaxTimeUs > 
                    _supervisorSysModInfoVector[_supervisorStats._slowestSysModIdx]._sysModStats._execMaxTimeUs)
            _supervisorStats._slowestSysModIdx = i;
    }

    // Second slowest SysMod
    _supervisorStats._secondSlowestSysModIdx = 0;
    for (int i = 1; i < _supervisorSysModInfoVector.size(); i++)
    {
        if ((i != _supervisorStats._slowestSysModIdx) &&
                    (_supervisorSysModInfoVector[i]._sysModStats._execMaxTimeUs > 
                                    _supervisorSysModInfoVector[_supervisorStats._secondSlowestSysModIdx]._sysModStats._execMaxTimeUs))
            _supervisorStats._secondSlowestSysModIdx = i;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Loop Timing - Get Stats String
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String SysManager::statsGetString()
{
    // Min and max values
    char maxMinStr[100];
    sprintf(maxMinStr, "Max %luuS Min %luuS", _supervisorStats._loopTimeMaxUs, _supervisorStats._loopTimeMinUs);

    // Average loop time
    char averageStr[30];
    strcpy(averageStr, "N/A");
    if (_supervisorStats._totalLoops > 0)
        sprintf(averageStr, "%4.2f", _supervisorStats._loopTimeAvgUs);

    // Slowest loop activity
    String slowestStr;
    if ((_supervisorSysModInfoVector.size() > 0) && (_supervisorSysModInfoVector[_supervisorStats._slowestSysModIdx]._sysModStats._execMaxTimeUs != 0))
        slowestStr = "Slowest " + _supervisorSysModInfoVector[_supervisorStats._slowestSysModIdx]._pSysMod->modNameStr() + " " + 
                        String((uint32_t)_supervisorSysModInfoVector[_supervisorStats._slowestSysModIdx]._sysModStats._execMaxTimeUs) + "uS";

    // Second slowest
    if ((_supervisorSysModInfoVector.size() > 0) && (_supervisorStats._secondSlowestSysModIdx != _supervisorStats._slowestSysModIdx) && 
                    (_supervisorSysModInfoVector[_supervisorStats._secondSlowestSysModIdx]._sysModStats._execMaxTimeUs != 0))
        slowestStr += ", " + _supervisorSysModInfoVector[_supervisorStats._secondSlowestSysModIdx]._pSysMod->modNameStr() + " " + 
                        String((uint32_t)_supervisorSysModInfoVector[_supervisorStats._secondSlowestSysModIdx]._sysModStats._execMaxTimeUs) + "uS";

    // Combine
    return String("Avg ") + averageStr + String("uS ") + maxMinStr + String(" ") + slowestStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Loop Timing - Clear Stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::statsClear()
{
    _serviceLoopInfo.clear();
    for (SysModInfo& sysModInfo : _supervisorSysModInfoVector)
    {
        sysModInfo._sysModStats.clear();
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
            LOG_I(MODULE_PREFIX, "setStatusChangeCB sysMod %s cbValid %d sysModFound OK", sysModName, statusChangeCB != NULL);
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
String SysManager::getDebugStr(const char* sysModName)
{
    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            return pSysMod->getDebugStr();
        }
    }
    return "";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send command to SysMod
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SysManager::sendCmdJSON(const char* sysModName, const char* cmdJSON)
{
    // See if the sysmod is in the list
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->modNameStr().equals(sysModName))
        {
            pSysMod->receiveCmdJSON(cmdJSON);
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiReset(String &reqStr, String& respStr)
{
    // Register that a restart is required but don't restart immediately
    // as the acknowledgement would not get through
    systemRestart();

    // Result
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

void SysManager::apiGetVersion(String &reqStr, String& respStr)
{
    // LOG_I(MODULE_PREFIX, "apiGetVersion");
    char versionJson[200];
    snprintf(versionJson, sizeof(versionJson), R"({"req":"%s","rslt":"ok","SystemName":"%s","SystemVersion":"%s","SerialNo":"%s","MAC":"%s"})", 
                reqStr.c_str(), _systemName.c_str(), _systemVersion.c_str(), _ricSerialNoStoredStr.c_str(), _systemUniqueString.c_str());
    respStr = versionJson;
}

void SysManager::apiGetSysModInfo(String &reqStr, String& respStr)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    respStr = getStatusJSON(sysModName.c_str());
}

void SysManager::apiGetSysModDebug(String &reqStr, String& respStr)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String debugStr = "\"debugStr\":\"" + getDebugStr(sysModName.c_str()) + "\"";
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, debugStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Friendly name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiFriendlyName(String &reqStr, String& respStr)
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

        // Store the new name (even if it is blank)
        String jsonConfig = getMutableConfigJson();
        if (_pMutableConfig)
        {
            _pMutableConfig->setConfigData(jsonConfig.c_str());
            _pMutableConfig->writeConfig();
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

void SysManager::apiSerialNumber(String &reqStr, String& respStr)
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
            _pMutableConfig->setConfigData(jsonConfig.c_str());
            _pMutableConfig->writeConfig();
        }
    }

    // Create response JSON
    char JsonOut[MAX_FRIENDLY_NAME_LENGTH + 70];
    snprintf(JsonOut, sizeof(JsonOut), R"("SerialNo":"%s")", _ricSerialNoStoredStr.c_str());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, JsonOut);
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
    // Show stats
    String infoStr;
    infoStr = 
        _systemName +
        " V" + _systemVersion +
        " Heap " + String(heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // TODO - get reportList from config and report accordingly

    String newStr = getDebugStr("NetworkManager");
    if (newStr.length() > 0)
        infoStr += " " + newStr;
    
    newStr = getDebugStr("BLEManager");
    if (newStr.length() > 0)
        infoStr += " " + newStr;

    // MiniHDLC info
    // MiniHDLCStats* pStats = _commandSerial.getHDLCStats();
    // infoStr +=
    //         " FR " + String(pStats->_rxFrameCount) + 
    //         " CRC " + String(pStats->_frameCRCErrCount) + 
    //         " TOO " + String(pStats->_frameTooLongCount) +
    //         " NOM " + String(pStats->_rxBufAllocFail);

    // // Robot controller info
    // infoStr += " " + _robotController.getDebugStr();

    // SysManager info
    newStr = statsGetString();
    if (newStr.length() > 0)
        infoStr += " " + newStr;

    newStr = getDebugStr("RobotController");
    if (newStr.length() > 0)
        infoStr += " " + newStr;

    // OTA update debug
    if (_pFirmwareUpdateSysMod && _pFirmwareUpdateSysMod->isBusy())
    {
        newStr = _pFirmwareUpdateSysMod->getDebugStr();
        if (newStr.length() > 0)
            infoStr += " " + newStr;
    }

    // Check if there's a callback registered for additional stats
    if (_statsCB)
    {
        newStr = _statsCB();
        if (newStr.length() > 0)
            infoStr += " " + newStr;
    }

    // Report stats
    LOG_I(MODULE_PREFIX, "%s", infoStr.c_str());
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

void SysManager::statusChangeBLEConnCB()
{
    // Get BLE status JSON
    ConfigBase statusBLE = getStatusJSON("BLEManager");
    bool bleIsConnected = statusBLE.getLong("isConn", 0) != 0;
    LOG_W(MODULE_PREFIX, "BLE connection change isConn %s", bleIsConnected ? "YES" : "NO");

    // Check if WiFi should be paused
    bool pauseWiFiForBLEConn = _sysModManConfig.getString("pauseWiFiforBLE", 0);
    if (pauseWiFiForBLEConn)
    {
        networkSystem.pauseWiFi(bleIsConnected);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process endpoint message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysManager::processEndpointMsg(ProtocolEndpointMsg &cmdMsg)
{
    // Handle the command message
    ProtocolMsgProtocol protocol = cmdMsg.getProtocol();

#ifdef DEBUG_ENDPOINT_MESSAGES
    // Debug
    LOG_I(MODULE_PREFIX, "Message received type %s msgNum %d dirn %d len %d", 
    		ProtocolEndpointMsg::getProtocolAsString(protocol), 
    		cmdMsg.getMsgNumber(), cmdMsg.getDirection(),
    		cmdMsg.getBufLen());
#endif

    // Handle ROSSerial
    if (protocol == MSG_PROTOCOL_ROSSERIAL)
    {
        // Handle ROSSerial messages by sending to the robot
        // Not implemented since move from RobotController & currently unused in this direction
        // return _pRobot->addCommand(cmdMsg);
    }
    else if (protocol == MSG_PROTOCOL_RICREST)
    {
        // Extract request msg
        RICRESTMsg ricRESTReqMsg;
        ricRESTReqMsg.decode(cmdMsg.getBuf(), cmdMsg.getBufLen());

#ifdef DEBUG_RICREST_MESSAGES
//         LOG_I(MODULE_PREFIX, "RICREST rx param1 %s", ricRESTReqMsg.getStringParam1().c_str());
#endif

        // Check elemCode of message
        String respMsg;
        switch(ricRESTReqMsg.getElemCode())
        {
            case RICRESTMsg::RICREST_REST_ELEM_URL:
            {
                processRICRESTURL(ricRESTReqMsg, respMsg);
                break;
            }
            case RICRESTMsg::RICREST_REST_ELEM_BODY:
            {
                processRICRESTBody(ricRESTReqMsg, respMsg);
                break;
            }
            case RICRESTMsg::RICREST_REST_ELEM_CMDRESPJSON:
            {
                // This message type is reserved for responses
                LOG_W(MODULE_PREFIX, "processEndpointMsg rx RICREST JSON reserved for response");
                break;
            }
            case RICRESTMsg::RICREST_REST_ELEM_COMMAND_FRAME:
            {
                processRICRESTCmdFrame(ricRESTReqMsg, respMsg, cmdMsg.getChannelID());
                break;
            }
            case RICRESTMsg::RICREST_REST_ELEM_FILEBLOCK:
            {
                processRICRESTFileBlock(ricRESTReqMsg, respMsg);
                break;
            }
        }

        // Check for response
        if (respMsg.length() != 0)
        {
            // Send the response back
            RICRESTMsg ricRESTRespMsg;
            ProtocolEndpointMsg endpointMsg;
            ricRESTRespMsg.encode(respMsg, endpointMsg);
            endpointMsg.setAsResponse(cmdMsg);

            // Send message on the appropriate channel
            if (_pProtocolEndpointManager)
                _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);

            // Debug
#ifdef DEBUG_RICREST_MESSAGES
            LOG_I(MODULE_PREFIX, "processEndpointMsg restAPI msgLen %d response %s", 
                        ricRESTReqMsg.getBinLen(), endpointMsg.getBuf());
#endif
        }
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysManager::processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    // Handle via standard REST API
    return _pRestAPIEndpointManager->handleApiRequest(ricRESTReqMsg.getReq().c_str(), respMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysManager::processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    // NOTE - implements POST for RICREST - not currently needed

//     // Handle the body
//     String reqStr;
//     uint32_t bufferPos = ricRESTReqMsg.getBufferPos();
//     const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
//     uint32_t bufferLen = ricRESTReqMsg.getBinLen();
//     uint32_t totalBytes = ricRESTReqMsg.getTotalBytes();
//     bool rsltOk = false;
//     if (pBuffer && _pRestAPIEndpointManager)
//     {
//         _pRestAPIEndpointManager->handleApiRequestBody(reqStr, pBuffer, bufferLen, bufferPos, totalBytes);
//         rsltOk = true;
//     }

//     // Response
//     Utils::setJsonBoolResult(pReqStr, respMsg, rsltOk);

//     // Debug
// // #ifdef DEBUG_RICREST_MESSAGES
//     LOG_I(MODULE_PREFIX, "addCommand restBody binBufLen %d bufferPos %d totalBytes %d", 
//                 bufferLen, bufferPos, totalBytes);
// // #endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysManager::processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, uint32_t channelID)
{
    // Handle command frames
    ConfigBase cmdFrame = ricRESTReqMsg.getPayloadJson();
    String cmdName = cmdFrame.getString("cmdName", "");

#ifdef DEBUG_RICREST_FILEUPLOAD
    LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame %s", cmdName.c_str());
#endif

    if (cmdName.equalsIgnoreCase("ufStart"))
    {
        fileUploadStart(ricRESTReqMsg.getReq(), respMsg, channelID, cmdFrame);
    }
    else if (cmdName.equalsIgnoreCase("ufEnd"))
    {
        fileUploadEnd(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
    } 
    else if (cmdName.equalsIgnoreCase("ufCancel"))
    {
        fileUploadCancel(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysManager::processRICRESTFileBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    // Handle the upload block
    uint32_t filePos = ricRESTReqMsg.getBufferPos();
    const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
    uint32_t bufferLen = ricRESTReqMsg.getBinLen();

    // Update state machine with block
    bool batchValid = false;
    bool isFinalBlock = false;
    bool genAck = false;
    _fileUploadHandler.blockRx(filePos, bufferLen, batchValid, isFinalBlock, genAck);

    // Debug
    if (isFinalBlock)
    {
        LOG_W(MODULE_PREFIX, "RICRESTFileBlock isFinal %d", isFinalBlock);
    }

    if (genAck)
    {
        // block returns true when an acknowledgement is required - so send that ack
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", _fileUploadHandler.getOkTo());
        Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "RICRESTFileBlock BatchOK Sending OkTo %d rxBlockFilePos %d len %d batchCount %d resp %s", 
                _fileUploadHandler.getOkTo(), filePos, bufferLen, _fileUploadHandler._batchBlockCount, respMsg.c_str());
#endif
    }
    else
    {
        // Check if we are actually uploading
        if (!_fileUploadHandler._isUploading)
        {
            LOG_W(MODULE_PREFIX, "RICRESTFileBlock NO UPLOAD IN PROGRESS filePos %d len %d", filePos, bufferLen);
            // Not uploading
            Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, "\"noUpload\"");
        }
        else
        {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
            LOG_I(MODULE_PREFIX, "RICRESTFileBlock filePos %d len %d batchCount %d resp %s", 
                    filePos, bufferLen, _fileUploadHandler._batchBlockCount, respMsg.c_str());
#endif
            // Just another block - don't ack yet
        }
    }

    // Check valid
    if (batchValid)
    {
        // Handle as a file block or firmware block
        if (_fileUploadHandler._fileIsRICFirmware) 
        {
            // Part of system firmware
            _pFirmwareUpdateSysMod->firmwareUpdateBlock(filePos, pBuffer, bufferLen);
        }
        else
        {
            // Part of a file
            fileSystem.uploadAPIBlockHandler("", _fileUploadHandler._reqStr, _fileUploadHandler._fileName, 
                                _fileUploadHandler._fileSize, filePos, pBuffer, bufferLen, isFinalBlock);
        }
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::serviceFileUpload()
{
#ifdef DEBUG_SHOW_FILE_UPLOAD_STATS
    // Stats display
    if (_fileUploadHandler.debugStatsReady())
    {
        LOG_I(MODULE_PREFIX, "fileUploadStats %s", _fileUploadHandler.debugStatsStr().c_str());
    }
#endif
    bool genBatchAck = false;
    _fileUploadHandler.service(genBatchAck);
    if (genBatchAck)
    {
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", _fileUploadHandler.getOkTo());
        String respMsg;
        Utils::setJsonBoolResult("ufBlock", respMsg, true, ackJson);

        // Send the response back
        RICRESTMsg ricRESTRespMsg;
        ProtocolEndpointMsg endpointMsg;
        ricRESTRespMsg.encode(respMsg, endpointMsg);
        endpointMsg.setAsResponse(_fileUploadHandler._commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_DIRECTION_RESPONSE);

        // Debug
        LOG_W(MODULE_PREFIX, "serviceFileUpload timed-out block receipt Sending OkTo %d batchCount %d", 
                _fileUploadHandler.getOkTo(), _fileUploadHandler._batchBlockCount);

        // Send message on the appropriate channel
        if (_pProtocolEndpointManager)
            _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);
    }
}

void SysManager::fileUploadStart(const String& reqStr, String& respMsg, uint32_t channelID, ConfigBase& cmdFrame)
{
    // Get params
    String ufStartReq = cmdFrame.getString("reqStr", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");

    // Start file upload handler
    uint32_t fileBlockSize = 0;
    uint32_t batchAckSize = 0;
    String errorMsg;
    bool startOk = _fileUploadHandler.start(ufStartReq, fileName, fileLen, fileType, 
                fileBlockSize, batchAckSize, channelID, errorMsg);

    // Check if firmware
    if (startOk && _fileUploadHandler._fileIsRICFirmware)
    {
        // Start ESP OTA update
        startOk = _pFirmwareUpdateSysMod->firmwareUpdateStart(_fileUploadHandler._fileName, 
                            _fileUploadHandler._fileSize);
        if (!startOk)
            errorMsg = "ESP OTA fail";
    }

    // Response
    char extraJson[100];
    snprintf(extraJson, sizeof(extraJson), "\"batchMsgSize\":%d,\"batchAckSize\":%d", fileBlockSize, batchAckSize);
    Utils::setJsonResult(reqStr.c_str(), respMsg, startOk, errorMsg.c_str(), extraJson);

    LOG_I(MODULE_PREFIX, "fileUploadStart reqStr %s filename %s fileType %s fileLen %d", 
                _fileUploadHandler._reqStr.c_str(),
                _fileUploadHandler._fileName.c_str(), 
                _fileUploadHandler._fileType.c_str(), 
                _fileUploadHandler._fileSize);
}

void SysManager::fileUploadEnd(const String& reqStr, String& respMsg, ConfigBase& cmdFrame)
{
    // Handle file end
#ifdef DEBUG_RICREST_FILEUPLOAD
    uint32_t blocksSent = cmdFrame.getLong("blockCount", 0);
#endif

    // Check if firmware
    bool rslt = true;
    if (_fileUploadHandler._fileIsRICFirmware)
    {
        // Start ESP OTA update
        rslt = _pFirmwareUpdateSysMod->firmwareUpdateEnd();
    }

    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respMsg, rslt);

    // Debug
    // LOG_I(MODULE_PREFIX, "fileUploadEnd blockCount %d blockRate %f rslt %s", 
    //                 _fileUploadHandler._blockCount, _fileUploadHandler.getBlockRate(), rslt ? "OK" : "FAIL");

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    LOG_I(MODULE_PREFIX, "fileUploadEnd reqStr %s fileName %s fileType %s fileLen %d blocksSent %d", 
                _fileUploadHandler._reqStr.c_str(),
                fileName.c_str(), 
                fileType.c_str(), 
                fileLen, 
                blocksSent);
#endif

    // End upload
    _fileUploadHandler.end();
}

void SysManager::fileUploadCancel(const String& reqStr, String& respMsg, ConfigBase& cmdFrame)
{
    // Handle file cancel
    String fileName = cmdFrame.getString("fileName", "");

    // Cancel upload
    _fileUploadHandler.cancel();

    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respMsg, true);

    // Debug
    LOG_I(MODULE_PREFIX, "fileUploadCancel fileName %s", fileName.c_str());

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    LOG_I(MODULE_PREFIX, "fileUploadCancel reqStr %s fileName %s", 
                _fileUploadHandler._reqStr.c_str(),
                fileName.c_str());
#endif
}
