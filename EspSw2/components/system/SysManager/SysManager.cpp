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
// #define DEBUG_RICREST_CMDFRAME
// #define DEBUG_LIST_SYSMODS
// #define DEBUG_SYSMOD_WITH_GLOBAL_VALUE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysManager::SysManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig)
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

    // Stream handler
    _pStreamHandlerSysMod = nullptr;

    // Status change callback
    _statsCB = NULL;

    // Supervisor vector needs an update
    _supervisorDirty = false;
    _serviceLoopCurModIdx = 0;

    // Extract info from config
    _sysModManConfig = pGlobalConfig ? 
                pGlobalConfig->getString(_moduleName.c_str(), "{}") :
                defaultConfig.getString(_moduleName.c_str(), "{}");

    // Extract system name from config
    _systemName = defaultConfig.getString("SystemName", "RBot");
    _systemVersion = defaultConfig.getString("SystemVersion", "0.0.0");

    // Monitoring period and monitoring timer
    _diagsLogPeriodMs = _sysModManConfig.getLong("monitorPeriodMs", 10000);
    _diagsLogTimerMs = millis();
    _sysModManConfig.getArrayElems("reportList", _diagsLogReportList);
    _diagsLogTimerStarted = false;

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
        _pRestAPIEndpointManager->addEndpoint("hwrevno", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                std::bind(&SysManager::apiHwRevisionNumber, this, std::placeholders::_1, std::placeholders::_2),
                "HW revision number");
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
        setStatusChangeCB("BLEMan", std::bind(&SysManager::statusChangeBLEConnCB, this));
    }

    // Setup file upload handler
    _fileUploadHandler.setup(_pProtocolEndpointManager);

    // Keep track of special-purpose sys-mods - assumes there is only
    // one of each special type
    _pStreamHandlerSysMod = nullptr;
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod)
        {
            if (pSysMod->isFirmwareUpdateModule())
            {
                _fileUploadHandler.setFWUpdater(pSysMod);
            }
            if (pSysMod->isStreamHandlerModule())
            {
                _pStreamHandlerSysMod = pSysMod;
            }
        }
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
       
    // Diagnostics log periodic output
    if (_diagsLogTimerStarted)
    {
        // Check if time to output diagnostic log is up
        if (Utils::isTimeout(millis(), _diagsLogTimerMs, _diagsLogPeriodMs))
        {
            // Calculate supervisory stats
            _supervisorStats.calculate();

            // Show stats
            statsShow();

            // Clear stats for start of next diagnostics period
            _supervisorStats.clear();

            // Wait until next period
            _diagsLogTimerMs = millis();
        }
    }
    else
    {
        // Start monitoring
        _supervisorStats.clear();
        _diagsLogTimerMs = millis();
        _diagsLogTimerStarted = true;
    }

    // Check the index into list of sys-mods to services is valid
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
        // Start SysMod timer
        _supervisorStats.execStarted(_serviceLoopCurModIdx);
        _sysModServiceVector[_serviceLoopCurModIdx]->service();
        _supervisorStats.execEnded(_serviceLoopCurModIdx);
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

    // Service upload
    _fileUploadHandler.service();
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
    // Check if it is the SysManager's own stats that are wanted
    if (strcasecmp(sysModName, "SysMan") == 0)
        return _supervisorStats.getSummaryString();

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

void SysManager::apiReset(const String &reqStr, String& respStr)
{
    // Register that a restart is required but don't restart immediately
    // as the acknowledgement would not get through
    systemRestart();

    // Result
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

void SysManager::apiGetVersion(const String &reqStr, String& respStr)
{
    char versionJson[225];
    snprintf(versionJson, sizeof(versionJson),
             R"({"req":"%s","rslt":"ok","SystemName":"%s","SystemVersion":"%s","SerialNo":"%s",)"
             R"("MAC":"%s","RicHwRevNo":%d})",
             reqStr.c_str(), _systemName.c_str(), _systemVersion.c_str(), _ricSerialNoStoredStr.c_str(),
             _systemUniqueString.c_str(), getHwRevision());
    respStr = versionJson;
}

void SysManager::apiGetSysModInfo(const String &reqStr, String& respStr)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    respStr = getStatusJSON(sysModName.c_str());
}

void SysManager::apiGetSysModDebug(const String &reqStr, String& respStr)
{
    // Get name of SysMod
    String sysModName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String debugStr = "\"debugStr\":\"" + getDebugStr(sysModName.c_str()) + "\"";
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, debugStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Friendly name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysManager::apiFriendlyName(const String &reqStr, String& respStr)
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

void SysManager::apiSerialNumber(const String &reqStr, String& respStr)
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

void SysManager::apiHwRevisionNumber(const String &reqStr, String& respStr)
{
    // Create response JSON
    char jsonOut[30];
    snprintf(jsonOut, sizeof(jsonOut), R"("RicHwRevNo":%d)", getHwRevision());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, jsonOut);
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
    char statsStr[700];
    snprintf(statsStr, sizeof(statsStr), "%s V%s Heap %d (Min %d)", 
                _systemName.c_str(),
                _systemVersion.c_str(),
                heap_caps_get_free_size(MALLOC_CAP_8BIT),
                heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));

    // Add stats
    for (String& srcStr : _diagsLogReportList)
    {
        String modStr = getDebugStr(srcStr.c_str());
        if (modStr.length() > 0)
        {
            strlcat(statsStr, " ", sizeof(statsStr));
            strlcat(statsStr, modStr.c_str(), sizeof(statsStr));
        }
    }

    // OTA update stats
    String newStr = _fileUploadHandler.getDebugStr();
    if (newStr.length() > 0)
    {
        strlcat(statsStr, " ", sizeof(statsStr));
        strlcat(statsStr, newStr.c_str(), sizeof(statsStr));
    }

    // Report stats
    LOG_I(MODULE_PREFIX, "%s", statsStr);
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
    ConfigBase statusBLE = getStatusJSON("BLEMan");
    bool bleIsConnected = statusBLE.getLong("isConn", 0) != 0;
    LOG_I(MODULE_PREFIX, "BLE connection change isConn %s", bleIsConnected ? "YES" : "NO");

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
    }
    else if (protocol == MSG_PROTOCOL_RICREST)
    {
        // Extract request msg
        RICRESTMsg ricRESTReqMsg;
        ricRESTReqMsg.decode(cmdMsg.getBuf(), cmdMsg.getBufLen());

#ifdef DEBUG_RICREST_MESSAGES
        LOG_I(MODULE_PREFIX, "RICREST rx elemCode %d", ricRESTReqMsg.getElemCode());
#endif

        // Check elemCode of message
        String respMsg;
        switch(ricRESTReqMsg.getElemCode())
        {
            case RICRESTMsg::RICREST_ELEM_CODE_URL:
            {
                processRICRESTURL(ricRESTReqMsg, respMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_BODY:
            {
                processRICRESTBody(ricRESTReqMsg, respMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON:
            {
                // This message type is reserved for responses
                LOG_W(MODULE_PREFIX, "processEndpointMsg rx RICREST JSON reserved for response");
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_COMMAND_FRAME:
            {
                processRICRESTCmdFrame(ricRESTReqMsg, respMsg, cmdMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_FILEBLOCK:
            {
                _fileUploadHandler.handleFileBlock(ricRESTReqMsg, respMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_STREAMBLOCK:
            {
                if (_pStreamHandlerSysMod)
                    _pStreamHandlerSysMod->handleStreamBlock(ricRESTReqMsg, respMsg);
                break;
            }
        }

        // Check for response
        if (respMsg.length() != 0)
        {
            // Send the response back
            ProtocolEndpointMsg endpointMsg;
            RICRESTMsg::encode(respMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
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

bool SysManager::processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
{
    // Handle command frames
    ConfigBase cmdFrame = ricRESTReqMsg.getPayloadJson();
    String cmdName = cmdFrame.getString("cmdName", "");

#ifdef DEBUG_RICREST_CMDFRAME
    LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame %s", cmdName.c_str());
#endif

    // Check file upload
    if (_fileUploadHandler.handleCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg))
        return true;

    // Check streaming
    if (_pStreamHandlerSysMod && _pStreamHandlerSysMod->procRICRESTCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg))
        return true;

    // Otherwise see if any SysMod wants to handle this
    for (SysModBase* pSysMod : _sysModuleList)
    {
        if (pSysMod->procRICRESTCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg))
            return true;
    }

    return false;
}

