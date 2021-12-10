/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manager for SysTypes
// Handles selection of system type from a set of JSON alternatives
//
// Rob Dobson 2019-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SysTypeManager.h"
#include "SysManager.h"
#include <RdJson.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>

static const char* MODULE_PREFIX = "SysTypeManager";

// #define DEBUG_SYS_TYPE_CONFIG
// #define DEBUG_SYS_TYPE_CONFIG_DETAIL
// #define DEBUG_GET_POST_SETTINGS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SysTypeManager::SysTypeManager(ConfigNVS& sysTypeConfig) :
        _sysTypeConfig(sysTypeConfig)
{
    _lastPostResultOk = false;
    _pSysManager = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SysTypeManager::setup(const char** pSysTypeConfigArrayStatic, int sysTypeConfigArrayLen)
{
    // Add configurations from array
    for (int configIdx = 0; configIdx < sysTypeConfigArrayLen; configIdx++)
    {
        _sysTypesList.push_back(pSysTypeConfigArrayStatic[configIdx]);
#ifdef DEBUG_SYS_TYPE_CONFIG_DETAIL
        LOG_I(MODULE_PREFIX, "setup sysTypeFromArray %s", pSysTypeConfigArrayStatic[configIdx]);
#endif
    }

    // If there is only one configuration in the list then choose that one as the static config
    // it may still be overridden by config from the non-volatile storage if there is any set
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

        // Debug
#ifdef DEBUG_SYS_TYPE_CONFIG_DETAIL
        LOG_I(MODULE_PREFIX, "setup sysTypeFromNVS %s", _sysTypeConfig.getConfigString().c_str());
#endif

        // Get type name
        _curSysTypeName = _sysTypeConfig.getString("SysType", "");

        // Debug
        LOG_I(MODULE_PREFIX, "Only 1 SysType so assuming that type = %s", _curSysTypeName.c_str());
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
    
    // Check blank which means current one
    const char* pNameToMatch = (strlen(pSysTypeName) == 0) ? _curSysTypeName.c_str() : pSysTypeName;
    LOG_I(MODULE_PREFIX, "Requesting %s%s, there are %d default types", pNameToMatch, 
        (strlen(pSysTypeName) == 0) ? " (blank was passed)" : "",
        _sysTypesList.size());

    // Find matching sysType
    for (ConfigBase sysTypeConfig : _sysTypesList)
    {
        String sysTypeName = sysTypeConfig.getString("SysType", "");
        // LOG_D(MODULE_PREFIX, "Testing %s against %s", pNameToMatch, sysTypeName.c_str());
        if (sysTypeName.equals(pNameToMatch))
        {
#ifdef DEBUG_SYS_TYPE_CONFIG
            LOG_I(MODULE_PREFIX, "Config for %s found", pNameToMatch);
            LOG_I(MODULE_PREFIX, "Config str %s", sysTypeConfig.getConfigString().c_str());
#endif
            respStr = sysTypeConfig.getConfigString();
            return true;
        }
    }
    LOG_W(MODULE_PREFIX, "Config for %s not found", pNameToMatch);
    respStr = "{}";
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Settings
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
#ifdef DEBUG_SYS_TYPE_CONFIG
        LOG_I(MODULE_PREFIX, "setSystemTypeConfig %s", configJson.c_str());
#endif

        // Check JSON is valid
        int numJsonTokens = 0;
        if (!RdJson::validateJson(configJson.c_str(), numJsonTokens))
        {
            LOG_E(MODULE_PREFIX, "setSysSettings JSON failed to parse %s", configJson.c_str());
            return false;
        }

        // Store the configuration permanently
        // LOG_W(MODULE_PREFIX, "SYS_TYPE_CONFIG currently %s", _sysTypeConfig.getConfigString().c_str());
        _sysTypeConfig.writeConfig(configJson);
        // LOG_W(MODULE_PREFIX, "SYS_TYPE_CONFIG now %s", _sysTypeConfig.getConfigString().c_str());

        // Get type name
        _curSysTypeName = _sysTypeConfig.getString("SysType", "");
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
                            std::bind(&SysTypeManager::apiGetSysTypes, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Get system types");

    // Get a SysType config
    endpointManager.addEndpoint("getSysTypeConfiguration", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                            std::bind(&SysTypeManager::apiGetSysTypeConfig, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Get config for a system type");

    // Post persisted settings - these settings override the underlying (static) SysType settings
    endpointManager.addEndpoint("postsettings", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&SysTypeManager::apiSysTypePostSettings, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Set settings for system",
                            "application/json", 
                            NULL,
                            RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                            NULL, 
                            std::bind(&SysTypeManager::apiSysTypePostSettingsBody, this, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    std::placeholders::_5, std::placeholders::_6),
                            NULL);

    // Get persisted settings - these are the overridden settings so may be empty
    endpointManager.addEndpoint("getsettings", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                            std::bind(&SysTypeManager::apiSysTypeGetSettings, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Get settings for system /getsettings/<filter> where filter is all, nv, base (nv indicates non-volatile) and filter can be blank for all");
}

void SysTypeManager::apiGetSysTypes(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "GetSysTypes");
    String sysTypesJson;
    getSysTypesListJSON(sysTypesJson);
    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, ("\"sysTypes\":" + sysTypesJson).c_str());
}

void SysTypeManager::apiGetSysTypeConfig(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
#ifdef DEBUG_GET_POST_SETTINGS
    LOG_I(MODULE_PREFIX, "apiGetSysTypeConfig");
#endif
    String sysTypeName = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String sysTypeJson;
    bool gotOk = getSysTypeConfig(sysTypeName.c_str(), sysTypeJson);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, gotOk, ("\"sysType\":" + sysTypeJson).c_str());
}

void SysTypeManager::apiSysTypeGetSettings(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    String filterSettings = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String settingsResp = "\"sysType\":\"" + _curSysTypeName + "\"";
#ifdef DEBUG_GET_POST_SETTINGS
    LOG_I(MODULE_PREFIX, "apiSysTypeGetSettings filter %s sysType %s", filterSettings.c_str(), _curSysTypeName.c_str());
#endif
    if (filterSettings.equalsIgnoreCase("nv") || filterSettings.equalsIgnoreCase("all") || (filterSettings == ""))
    {
        String sysSettingsJson = _sysTypeConfig.getPersistedConfig();
#ifdef DEBUG_GET_POST_SETTINGS
        LOG_I(MODULE_PREFIX, "apiSysTypeGetSettings nv %s", sysSettingsJson.c_str());
#endif
        settingsResp += ",\"nv\":" + sysSettingsJson;
    }
    if (filterSettings.equalsIgnoreCase("base") || filterSettings.equalsIgnoreCase("all") || (filterSettings == ""))
    {
        settingsResp += ",\"base\":" + _sysTypeConfig.getStaticConfig();
    }
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true, settingsResp.c_str());
}

void SysTypeManager::apiSysTypePostSettings(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
#ifdef DEBUG_GET_POST_SETTINGS
    LOG_I(MODULE_PREFIX, "PostSettings request %s", reqStr.c_str());
#endif
    String rebootRequired = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    if (_lastPostResultOk && rebootRequired.equalsIgnoreCase("reboot"))
    {
        LOG_I(MODULE_PREFIX, "PostSettings rebooting ... request %s", reqStr.c_str());
        if (_pSysManager)
            _pSysManager->systemRestart();
    }

    // Result
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, _lastPostResultOk);
    _lastPostResultOk = false;
}

void SysTypeManager::apiSysTypePostSettingsBody(const String& reqStr, const uint8_t *pData, size_t len, 
                size_t index, size_t total, const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "apiSysTypePostSettingsBody len %d index %d total %d curBufLen %d", len, index, total, _postResultBuf.size());
    if (len == total)
    {
        // Store the settings
        _lastPostResultOk = setSysSettings(pData, len);
        return;
    }

    // Check if first block
    if (index == 0)
        _postResultBuf.clear();

    // Append to the existing buffer
    _postResultBuf.insert(_postResultBuf.end(), pData, pData+len);

    // Check for complete
    if (_postResultBuf.size() == total)
    {
        // Store the settings from buffer and clear the buffer
        _lastPostResultOk = setSysSettings(_postResultBuf.data(), _postResultBuf.size());
        _postResultBuf.clear();
    }
}
