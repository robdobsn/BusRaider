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
#include <ConfigBase.h>
#include <list>

class RestAPIEndpointManager;

class SysTypeManager
{
public:
    // Constructor
    SysTypeManager();

    // Setup
    void setup(const char** pSysTypeConfigArray, int sysTypeConfigArrayLen, ConfigBase* pSysTypeConfig);

    // Add endpoints
    void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager);

    // Handling of SysTypes list
    void getSysTypesListJSON(String& sysTypesListJSON);
    bool getSysTypeConfig(const char* sysTypeName, String& sysTypeConfig);

    // Get and set SysSettings which are generally non-volatile and contain one of the SysType values
    void getSysSettings(String& respStr);
    bool setSysSettings(const uint8_t *pData, int len);

    // Get pointer to base settings (which may be hooked to handle config changes)
    ConfigBase* getConfigBasePtr()
    {
        return _pSysSettings;
    }

private:
    // List of sysTypes
    std::list<String> _sysTypesList;

    // Non-volatile settings
    ConfigBase* _pSysSettings;

    // API System type
    void apiGetSysTypes(String &reqStr, String &respStr);
    void apiGetSysTypeConfig(String &reqStr, String &respStr);

    // API System settings
    void apiSysTypeGetSettings(String &reqStr, String &respStr);
    void apiSysTypePostSettings(String &reqStr, String &respStr);
    void apiSysTypePostSettingsBody(String& reqStr, const uint8_t *pData, size_t len, size_t index, size_t total);
};
