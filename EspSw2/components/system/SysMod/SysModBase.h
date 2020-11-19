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

typedef std::function<void()> SysMod_statusChangeCB;

class SysManager;
class RestAPIEndpointManager;
class ProtocolEndpointManager;
class ProtocolEndpointMsg;
class RICRESTMsg;

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

    // Static function to define the manager for system modules
    static void setSysManager(SysManager* pSysManager)
    {
        _pSysManager = pSysManager;
    }
    SysManager* getSysManager()
    {
        return _pSysManager;
    }

    // Logging destination functionality - ensure no Log calls are made while logging!
    virtual void logSilently(const char* pLogStr)
    {
    }

    // Get debug string
    virtual String getDebugStr()
    {
        return "";
    }

    // Direct firmware update SysMod
    virtual bool isFirmwareUpdateModule()
    {
        return false;
    }

    // Firmware update
    virtual bool firmwareUpdateStart(const char* fileName, size_t fileLen)
    {
        return false;
    }
    virtual bool firmwareUpdateBlock(uint32_t filePos, const uint8_t *pBlock, size_t blockLen)
    {
        return false;
    }
    virtual bool firmwareUpdateEnd()
    {
        return false;
    }

    // Process RICRESTMsg CmdFrame
    virtual bool procRICRESTCmdFrame(const String& cmdName, const RICRESTMsg& ricRESTReqMsg, 
                String& respMsg, const ProtocolEndpointMsg &endpointMsg)
    {
        return false;
    }

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

protected:
    // Default, global and mutable configs
    ConfigMulti _combinedConfig;

    // Execute status change callbacks
    void executeStatusChangeCBs();

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
