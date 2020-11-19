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

#pragma once 

#include "SysModStats.h"
#include "SysModBase.h"
#include <list>
#include <vector>
#include <RestAPIEndpointManager.h>
#include <ProtocolEndpointManager.h>
#include "FileUploadHandler.h"

typedef String (*SysManager_statsCB)();

class ConfigBase;
class RestAPIEndpointManager;
class RICRESTMsg;

class SysManager
{
public:
    // Constructor
    SysManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pMutableConfig);

    // Setup
    void setup();

    // Service
    void service();

    // Manage machines
    void add(SysModBase* pSysMod);

    // Get system name
    String getSystemName()
    {
        return _systemName;
    }

    // Get friendly name
    String getFriendlyName();
    bool getFriendlyNameIsSet();

    // Set system unique string
    void setSystemUniqueString(const char* sysUniqueStr)
    {
        _systemUniqueString = sysUniqueStr;
    }

    // Get stats
    String statsGetString();

    // Set stats callback (for SysManager's own stats)
    void setStatsCB(SysManager_statsCB statsCB)
    {
        _statsCB = statsCB;
    }

    // Add status change callback on a SysMod
    void setStatusChangeCB(const char* sysModName, SysMod_statusChangeCB statusChangeCB);

    // Get status from SysMod
    String getStatusJSON(const char* sysModName);

    // Get debug from SysMod
    String getDebugStr(const char* sysModName);

    // Send command to SysMod
    void sendCmdJSON(const char* sysModName, const char* cmdJSON);

    // Request system restart
    void systemRestart()
    {
        // Actual restart occurs within service routine after a short delay
        _systemRestartPending = true;
        _systemRestartMs = millis();
    }

    // REST API Endpoints
    void setRestAPIEndpoints(RestAPIEndpointManager& restAPIEndpoints)
    {
        _pRestAPIEndpointManager = &restAPIEndpoints;
    }

    RestAPIEndpointManager* getRestAPIEndpoints()
    {
        return _pRestAPIEndpointManager;
    }

    // ProtocolEndpointManager
    void setProtocolEndpointManager(ProtocolEndpointManager& protocolEndpointManager)
    {
        _pProtocolEndpointManager = &protocolEndpointManager;
    }

    ProtocolEndpointManager* getProtocolEndpointManager()
    {
        return _pProtocolEndpointManager;
    }

private:

    // Name of this module
    String _moduleName;

    // List of modules
    std::list<SysModBase*> _sysModuleList;

    // Firmware update sysMod
    SysModBase* _pFirmwareUpdateSysMod;

private:

    // Consts
    static const uint32_t RIC_SERIAL_NUMBER_BYTES = 16;
    static constexpr const char* RIC_SERIAL_SET_MAGIC_STR = "RoboticalMagic";

    // Supervisory info
    class SysModInfo
    {
    public:
        SysModInfo(SysModBase* pSysMod)
        {
            _pSysMod = pSysMod;
        }
        SysModBase* _pSysMod;
        SysModStats _sysModStats;
    };

    // Supervisor
    void supervisorSetup();
    bool _supervisorDirty;
    std::vector<SysModInfo> _supervisorSysModInfoVector;
    uint32_t _supervisorCurModIdx = 0;
    
    // Service loop info
    class ServiceLoopInfo
    {
    public:
        ServiceLoopInfo()
        {
            clear();
        }
        void clear()
        {
            _loopTimeAvgSum = 0;
            _loopTimeAvgCount = 0;
            _loopTimeMax = 0;
            _loopTimeMin = 0;
            _lastLoopStartMicros = 0;
        }
        void startLoop();
        void endLoop();
        unsigned long _loopTimeAvgSum;
        unsigned long _loopTimeAvgCount;
        unsigned long _loopTimeMax;
        unsigned long _loopTimeMin;
        uint64_t _lastLoopStartMicros;
    };
    ServiceLoopInfo _serviceLoopInfo;

    // Supervisor stats
    class SupervisorStats
    {
    public:
        SupervisorStats()
        {
            clear();
        }
        void clear()
        {
            _slowestSysModIdx = 0;
            _secondSlowestSysModIdx = 0;
            _loopTimeMinUs = 0;
            _loopTimeMaxUs = 0;
            _loopTimeAvgUs = 0;
            _totalLoops = 0;
        }
        int _slowestSysModIdx;
        int _secondSlowestSysModIdx;
        unsigned long _loopTimeMinUs;
        unsigned long _loopTimeMaxUs;
        unsigned long _totalLoops;
        double _loopTimeAvgUs;
    };
    SupervisorStats _supervisorStats;

    // Monitor timer and period
    unsigned long _monitorPeriodMs;
    unsigned long _monitorTimerMs;
    bool _monitorTimerStarted;
    std::vector<String> _monitorReportList;

    // Stats available callback
    SysManager_statsCB _statsCB;

    // Stats
    void statsClear();
    void statsUpdate();
    void statsShow();

private:
    // SysManager also handles system restarts
    bool _systemRestartPending;
    unsigned long _systemRestartMs;
    static const int SYSTEM_RESTART_DELAY_MS = 1000;

private:
    // System name and version
    String _systemName;
    String _systemVersion;

    // Module config
    ConfigBase _sysModManConfig;

    // Mutable config
    ConfigBase* _pMutableConfig;
    String _defaultFriendlyName;
    bool _defaultFriendlyNameIsSet;
    String _friendlyNameStored;
    bool _friendlyNameIsSet;
    String _ricSerialNoStoredStr;


    // Unique string for this system
    String _systemUniqueString;
    
    // Friendly name max len
    static const uint32_t MAX_FRIENDLY_NAME_LENGTH = 60;

private:
    // Endpoints
    RestAPIEndpointManager* _pRestAPIEndpointManager;

    // Protocol endpoints
    ProtocolEndpointManager* _pProtocolEndpointManager;
    
private:
    // API to reset system
    void apiReset(const String &reqStr, String& respStr);

    // Get system version
    void apiGetVersion(const String &reqStr, String& respStr);

    // Friendly name get/set
    void apiFriendlyName(const String &reqStr, String& respStr);

    // Serial no
    void apiSerialNumber(const String &reqStr, String& respStr);

    // SysMod info and debug
    void apiGetSysModInfo(const String &reqStr, String& respStr);
    void apiGetSysModDebug(const String &reqStr, String& respStr);

    // Clear status change callbacks
    void clearAllStatusChangeCBs();

    // Connection change on BLE
    void statusChangeBLEConnCB();

    // Process endpoint message
    bool processEndpointMsg(ProtocolEndpointMsg& msg);
    bool processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg);
    bool processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg);
    bool processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                        const ProtocolEndpointMsg &endpointMsg);
    bool processRICRESTFileBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg);
    void serviceFileUpload();
    void fileUploadStart(const String& reqStr, String& respMsg, uint32_t channelID, ConfigBase& cmdFrame);
    void fileUploadEnd(const String& reqStr, String& respMsg, ConfigBase& cmdFrame);
    void fileUploadCancel(const String& reqStr, String& respMsg, ConfigBase& cmdFrame);

    // Mutable config
    String getMutableConfigJson();

private:
    // File upload
    FileUploadHandler _fileUploadHandler;    
};
