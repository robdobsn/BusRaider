// RBotFirmware
// Rob Dobson 2019

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base class for SysMods (System Modules)
// For more info see SysManager
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "SysModBase.h"
#include <SysManager.h>
#include <ConfigPinMap.h>

SysManager* SysModBase::_pSysManager = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysModBase::SysModBase(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig, const char* pGlobalConfigPrefix)
{
    // Set sysmod name
    if (pModuleName)
        _sysModName = pModuleName;
    _sysModLogPrefix = _sysModName + ": ";

    // Handle config
    String moduleConfigPrefix = _sysModName;
    if (pGlobalConfigPrefix)
        moduleConfigPrefix = pGlobalConfigPrefix;
    _combinedConfig.addConfig(&defaultConfig, moduleConfigPrefix.c_str(), false);
    _combinedConfig.addConfig(pGlobalConfig, moduleConfigPrefix.c_str(), false);
    _combinedConfig.addConfig(pMutableConfig, "", true);

    // Set log level for module if specified
    String logLevel = configGetString("logLevel", "");
    setModuleLogLevel(pModuleName, logLevel);

    // Add to system module manager
    if (_pSysManager)
        _pSysManager->add(this);
}

SysModBase::~SysModBase()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String SysModBase::getSystemName()
{
    if (_pSysManager)
        return _pSysManager->getSystemName();
    return "RIC";
}

String SysModBase::getFriendlyName()
{
    if (_pSysManager)
        return _pSysManager->getFriendlyName();
    return "RIC";
}

RestAPIEndpointManager* SysModBase::getRestAPIEndpoints()
{
    // Check parent
    if (!_pSysManager)
        return nullptr;

    // Get parent's endpoints
    return _pSysManager->getRestAPIEndpoints();
}

ProtocolEndpointManager* SysModBase::getProtocolEndpointManager()
{
    // Check parent
    if (!_pSysManager)
        return nullptr;

    // Get endpoint manager
    return _pSysManager->getProtocolEndpointManager();
}

long SysModBase::configGetLong(const char *dataPath, long defaultValue)
{
    return _combinedConfig.getLong(dataPath, defaultValue);
}

String SysModBase::configGetString(const char *dataPath, const char* defaultValue)
{
    return _combinedConfig.getString(dataPath, defaultValue);
}

String SysModBase::configGetString(const char *dataPath, const String& defaultValue)
{
    return _combinedConfig.getString(dataPath, defaultValue);
}

void SysModBase::configRegisterChangeCallback(ConfigChangeCallbackType configChangeCallback)
{
    _combinedConfig.registerChangeCallback(configChangeCallback);
}

int SysModBase::configGetPin(const char* dataPath, const char* defaultValue)
{
    String pinName = configGetString(dataPath, defaultValue);
    return ConfigPinMap::getPinFromName(pinName.c_str());
}

void SysModBase::configSaveData(const String& configStr)
{
    _combinedConfig.writeConfig(configStr);
}

// Get JSON status of another SysMod
String SysModBase::sysModGetStatusJSON(const char* sysModName)
{
    if (_pSysManager)
        return _pSysManager->getStatusJSON(sysModName);
    return "{\"rslt\":\"fail\"}";
}

// Post JSON command to another SysMod
void SysModBase::sysModSendCmdJSON(const char* sysModName, const char* jsonCmd)
{
    if (_pSysManager)
        _pSysManager->sendCmdJSON(sysModName, jsonCmd);
}

// Add status change callback on a SysMod
void SysModBase::sysModSetStatusChangeCB(const char* sysModName, SysMod_statusChangeCB statusChangeCB)
{
    if (_pSysManager)
        _pSysManager->setStatusChangeCB(sysModName, statusChangeCB);
}

// Execute status change callbacks
void SysModBase::executeStatusChangeCBs()
{
    for (SysMod_statusChangeCB& statusChangeCB : _statusChangeCBs)
    {
        if (statusChangeCB)
            statusChangeCB();
    }
}

SupervisorStats* SysModBase::getSysManagerStats()
{
    if (_pSysManager)
        return _pSysManager->getStats();
    return nullptr;
}
