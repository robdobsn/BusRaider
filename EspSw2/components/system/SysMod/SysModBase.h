// RBotFirmware
// Rob Dobson 2019

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base class for SysMods (System Modules)
// For more info see SysManager
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <functional>
#include <ConfigMulti.h>
#include <list>
#include <Logger.h>

// Forward declarations
class SysManager;
class RestAPIEndpointManager;
class ProtocolEndpointManager;
class ProtocolEndpointMsg;
class RICRESTMsg;
class SupervisorStats;
class FileStreamBlock;

// Status change callback function type
typedef std::function<void(const String& sourceName, bool changeToOnline)> SysMod_statusChangeCB;

// Message generator callback function type
typedef std::function<bool(const char* messageName, ProtocolEndpointMsg& msg)> SysMod_publishMsgGenFn;

// State change detector callback function type
typedef std::function<void(const char* stateName, std::vector<uint8_t>& stateHash)> SysMod_stateDetectCB;

class SysModBase
{
public:
    SysModBase(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig, const char* pGlobalConfigPrefix = NULL);
    virtual ~SysModBase();

    // Setup
    virtual void setup()
    {
    }

    // Add REST API endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
    {
    }

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
    {
    }

    // Service
    virtual void service()
    {
    }

    // Post-setup - called after setup of all sysMods complete
    virtual void postSetup()
    {
    }

    // Get name
    virtual const char* modName()
    {
        return _sysModName.c_str();
    }
    virtual String& modNameStr()
    {
        return _sysModName;
    }

    // Check if main activity busy
    virtual bool isBusy()
    {
        return false;
    }
    
    // System name
    virtual String getSystemName();

    // Friendly name
    virtual String getFriendlyName();

    // Config access
    virtual long configGetLong(const char *dataPath, long defaultValue);
    virtual String configGetString(const char *dataPath, const char* defaultValue);
    virtual String configGetString(const char *dataPath, const String& defaultValue);
    virtual bool configGetArrayElems(const char *dataPath, std::vector<String>& strList) const;
    virtual int configGetPin(const char* dataPath, const char* defaultValue);
    virtual void configRegisterChangeCallback(ConfigChangeCallbackType configChangeCallback);
    virtual ConfigBase& configGetConfig()
    {
        return _combinedConfig;
    }

    // Get JSON status string
    virtual String getStatusJSON()
    {
        return "{\"rslt\":\"ok\"}";
    }

    // Receive JSON command
    virtual void receiveCmdJSON(const char* cmdJSON)
    {
    }

    // Receive msg generator callback function
    virtual void receiveMsgGenCB(const char* msgGenID, SysMod_publishMsgGenFn msgGenCB, SysMod_stateDetectCB stateDetectCB)
    {
    }

    // Static function to define the manager for system modules
    static void setSysManager(SysManager* pSysManager)
    {
        _pSysManager = pSysManager;
    }
    SysManager* getSysManager()
    {
        return _pSysManager;
    }
    SupervisorStats* getSysManagerStats();

    // Logging destination functionality - ensure no Log calls are made while logging!
    virtual void logSilently(const char* pLogStr)
    {
    }

    // Get debug string
    virtual String getDebugJSON()
    {
        return "{}";
    }

    // File/Stream Start
    virtual bool fileStreamStart(const char* fileName, size_t fileLen)
    {
        return false;
    }
    virtual bool fileStreamDataBlock(FileStreamBlock& fileStreamBlock)
    {
        return false;
    }
    virtual bool fileStreamCancelEnd(bool isNormalEnd)
    {
        return true;
    }

    // File/stream system activity - main firmware update
    bool isSystemMainFWUpdate();

    // File/stream system activity - file transfer
    bool isSystemFileTransferring();

    // File/stream system activity - streaming
    bool isSystemStreaming();

public:
    // Non-virtual methods

    // Get REST API endpoints
    RestAPIEndpointManager* getRestAPIEndpoints();

    // Get ProtocolEndpointManager
    ProtocolEndpointManager* getProtocolEndpointManager();

    // Add status change callback on another SysMod
    void sysModSetStatusChangeCB(const char* sysModName, SysMod_statusChangeCB statusChangeCB);

    // Get JSON status of another SysMod
    String sysModGetStatusJSON(const char* sysModName);

    // Send JSON command to another SysMod
    void sysModSendCmdJSON(const char* sysModName, const char* jsonCmd);

    // Status change callback
    void setStatusChangeCB(SysMod_statusChangeCB statusChangeCB)
    {
        _statusChangeCBs.push_back(statusChangeCB);
    }

    // Clear status change callbacks
    void clearStatusChangeCBs()
    {
        _statusChangeCBs.clear();
    }

    // Set log level of module
    static void setModuleLogLevel(const char* pModuleName, const String& logLevel)
    {
        if (logLevel.startsWith("N"))
            esp_log_level_set(pModuleName, ESP_LOG_NONE);
        else if (logLevel.startsWith("E"))
            esp_log_level_set(pModuleName, ESP_LOG_ERROR);
        else if (logLevel.startsWith("W"))
            esp_log_level_set(pModuleName, ESP_LOG_WARN);
        else if (logLevel.startsWith("I"))
            esp_log_level_set(pModuleName, ESP_LOG_INFO);
        else if (logLevel.startsWith("D"))
            esp_log_level_set(pModuleName, ESP_LOG_DEBUG);
        else if (logLevel.startsWith("V"))
            esp_log_level_set(pModuleName, ESP_LOG_VERBOSE);
    }

protected:
    // Default, global and mutable configs
    ConfigMulti _combinedConfig;

    // Execute status change callbacks
    void executeStatusChangeCBs(bool changeToOn);

    // Mutable config management
    virtual void configSaveData(const String& pConfigStr);

private:
    // Name of this module
    String _sysModName;
    String _sysModLogPrefix;

    // Config prefix
    String _configPrefix;
    
    // Manager (parent)
    static SysManager* _pSysManager;

    // Status change callbacks
    std::list<SysMod_statusChangeCB> _statusChangeCBs;

};
