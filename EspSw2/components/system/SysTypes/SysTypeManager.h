/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manager for SysTypes
// Handles selection of system type from a set of JSON alternatives
//
// Rob Dobson 2019-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ConfigNVS.h>
#include <list>
#include "SpiramAwareAllocator.h"

class RestAPIEndpointManager;
class APISourceInfo;
class SysManager;

class SysTypeManager
{
public:
    // Constructor
    SysTypeManager(ConfigNVS& sysTypeConfig);

    // Setup
    void setup(const char** pSysTypeConfigArrayStatic, int sysTypeConfigArrayLen);

    // Add endpoints
    void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager);

    // Handling of SysTypes list
    void getSysTypesListJSON(String& sysTypesListJSON);
    bool getSysTypeConfig(const char* sysTypeName, String& sysTypeConfig);

    // Set SysSettings which are generally non-volatile and contain one of the SysType values
    bool setSysSettings(const uint8_t *pData, int len);

    // Set SysManager
    void setSysManager(SysManager* pSysManager)
    {
        _pSysManager = pSysManager;
    }

private:
    // List of sysTypes
    std::list<const char*> _sysTypesList;

    // Current sysType name
    String _curSysTypeName;

    // System type configuration
    ConfigNVS& _sysTypeConfig;

    // Last post result ok
    bool _lastPostResultOk;
    std::vector<uint8_t, SpiramAwareAllocator<uint8_t>> _postResultBuf;

    // SysManager
    SysManager* _pSysManager;

    // API System type
    void apiGetSysTypes(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiGetSysTypeConfig(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // API System settings
    void apiSysTypeGetSettings(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiSysTypePostSettings(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);
    void apiSysTypePostSettingsBody(const String& reqStr, const uint8_t *pData, size_t len, 
                        size_t index, size_t total, const APISourceInfo& sourceInfo);
};
