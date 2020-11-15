/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manager for SysTypes
// Handles selection of system type from a set of JSON alternatives
//
// Rob Dobson 2019-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SysTypeManager.h"
#include <RdJson.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>

static const char* MODULE_PREFIX = "SysTypeManager";

// #define DEBUG_SYS_TYPE_CONFIG

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysTypeManager::SysTypeManager(ConfigNVS& sysTypeConfig) :
        _sysTypeConfig(sysTypeConfig)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysTypeManager::setup(const char** pSysTypeConfigArrayStatic, int sysTypeConfigArrayLen)
{
    // Add configurations from array
    for (int configIdx = 0; configIdx < sysTypeConfigArrayLen; configIdx++)
        _sysTypesList.push_back(pSysTypeConfigArrayStatic[configIdx]);

    // If there is only one configuration in the list then choose that one
    if (_sysTypesList.size() == 1)
    {
        // Check JSON is valid
        int numJsonTokens = 0;
        if (!RdJson::validateJson(_sysTypesList.front(), numJsonTokens))
        {
            LOG_E(MODULE_PREFIX, "setup JSON from SysType failed to parse");
            return;
        }

        // Set config from first item
        _sysTypeConfig.setStaticConfigData(_sysTypesList.front());

        // Get type name for debug
        String sysTypeName = _sysTypeConfig.getString("SysType", "");

        // Debug
        LOG_I(MODULE_PREFIX, "Only 1 SysType so assuming that type = %s", sysTypeName.c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get list of SysTypes JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysTypeManager::getSysTypesListJSON(String &respStr)
{
    // Get a JSON formatted list of sysTypes
    respStr = "[";
    bool isFirst = true;
    for (ConfigBase sysTypeConfig : _sysTypesList)
    {
        // Get name of SysType
        String sysTypeName = sysTypeConfig.getString("SysType", "");

        // Add comma if needed
        if (!isFirst)
            respStr += ",";
        isFirst = false;

        // Append to return string
        respStr += "\"" + sysTypeName + "\"";
    }
    respStr += "]";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON for a SysTypeConfig
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SysTypeManager::getSysTypeConfig(const char* pSysTypeName, String &respStr)
{
    LOG_I(MODULE_PREFIX, "Requesting %s, there are %d default types", pSysTypeName, _sysTypesList.size());
    for (ConfigBase sysTypeConfig : _sysTypesList)
    {
        String sysTypeName = sysTypeConfig.getString("SysType", "");
        // LOG_D(MODULE_PREFIX, "Testing %s against %s", pSysTypeName, sysTypeName.c_str());
        if (sysTypeName.equals(pSysTypeName))
        {
#ifdef DEBUG_SYS_TYPE_CONFIG
            LOG_I(MODULE_PREFIX, "Config for %s found", pSysTypeName);
            LOG_I(MODULE_PREFIX, "Config str %s", sysTypeConfig.c_str());
#endif
            sysTypeConfig.getConfigString(respStr);
            return true;
        }
    }
    LOG_W(MODULE_PREFIX, "Config for %s not found", pSysTypeName);
    respStr = "{}";
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Settings
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysTypeManager::getSysSettings(String& respStr)
{
    _sysTypeConfig.getConfigString(respStr);
}

bool SysTypeManager::setSysSettings(const uint8_t *pData, int len)
{
    // Debug
#ifdef DEBUG_SYS_TYPE_CONFIG
    LOG_I(MODULE_PREFIX, "setSystemTypeConfig len %d", len);
#endif

    // Check sensible length
    if (len + 10 > _sysTypeConfig.getMaxLen())
    {
        LOG_W(MODULE_PREFIX, "setSysSettings config too long");
        return false;
    }

    // Extract string
    String configJson;
    if (Utils::strFromBuffer(pData, len, configJson))
    {
        // Store the configuration permanently
        _sysTypeConfig.writeConfig(configJson);
    }
    else
    {
        LOG_W(MODULE_PREFIX, "setSysSettings failed to set");
        return false;
    }

    // Ok
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysTypeManager::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    // Get SysTypes list
    endpointManager.addEndpoint("getSysTypes", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                            std::bind(&SysTypeManager::apiGetSysTypes, this, std::placeholders::_1, std::placeholders::_2),
                            "Get robot types");

    // Get a SysType config
    endpointManager.addEndpoint("getSysTypeConfiguration", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                            std::bind(&SysTypeManager::apiGetSysTypeConfig, this, std::placeholders::_1, std::placeholders::_2),
                            "Get config for a robot type");

    // Post settings
    endpointManager.addEndpoint("postsettings", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&SysTypeManager::apiSysTypePostSettings, this, std::placeholders::_1, std::placeholders::_2),
                            "Set settings for robot",
                            "application/json", 
                            NULL,
                            RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                            NULL, 
                            std::bind(&SysTypeManager::apiSysTypePostSettingsBody, this, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    std::placeholders::_5),
                            NULL);

    // Get settings
    endpointManager.addEndpoint("getsettings", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                            std::bind(&SysTypeManager::apiSysTypeGetSettings, this, std::placeholders::_1, std::placeholders::_2),
                            "Get settings for robot");
}

void SysTypeManager::apiGetSysTypes(const String &reqStr, String &respStr)
{
    LOG_I(MODULE_PREFIX, "GetSysTypes");
    String sysTypesJson;
    getSysTypesListJSON(sysTypesJson);
    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, ("\"sysTypes\":" + sysTypesJson).c_str());
}

void SysTypeManager::apiGetSysTypeConfig(const String &reqStr, String &respStr)
{
    LOG_I(MODULE_PREFIX, "GetSysTypeConfig");
    String sysTypeName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String sysTypeJson;
    bool gotOk = getSysTypeConfig(sysTypeName.c_str(), sysTypeJson);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, gotOk, ("\"sysType\":" + sysTypeJson).c_str());
}

void SysTypeManager::apiSysTypeGetSettings(const String &reqStr, String &respStr)
{
    LOG_V(MODULE_PREFIX, "GetSettings %s", respStr.c_str());
    String sysSettingsJson;
    getSysSettings(sysSettingsJson);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, ("\"cfg\":" + sysSettingsJson).c_str());
}

void SysTypeManager::apiSysTypePostSettings(const String &reqStr, String &respStr)
{
    LOG_I(MODULE_PREFIX, "PostSettings %s", reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

void SysTypeManager::apiSysTypePostSettingsBody(const String& reqStr, const uint8_t *pData, size_t len, size_t index, size_t total)
{
    LOG_I(MODULE_PREFIX, "PostSettingsBody len %d", len);
    // Store the settings
    setSysSettings(pData, len);
}
