/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigNVS
// Configuration persisted to non-volatile storage
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
class RdPreferences;

class ConfigNVS : public ConfigBase
{
public:
    ConfigNVS(const char* configNamespace, int configMaxlen, bool cacheConfig);
    virtual ~ConfigNVS();

    // Set the config data from a static source - note that only the
    // pointer is stored so this data MUST be statically allocated
    void setStaticConfigData(const char* pStaticConfig)
    {
        _pStaticConfig = pStaticConfig;
    }

    // Get config raw string
    virtual void getConfigString(String& configStr) const override final
    {
        if (_cacheConfig)
        {
            configStr = _dataStrJSON;
        }
        else if (_nonVolatileStoreEmpty && _pStaticConfig)
        {
            configStr = _pStaticConfig;
        }
        else
        {
            getNVConfigStr(configStr);
        }
    }

    // Clear
    virtual void clear() override final;

    // Initialise
    virtual bool setup() override final;

    // Write configuration string
    virtual bool writeConfig(const String& configJSONStr) override final;

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback) override final;

private:
    // Namespace used for Preferences lib
    String _configNamespace;

    // Preferences instance
    RdPreferences* _pPreferences;

    // List of callbacks on change of config
    std::vector<ConfigChangeCallbackType> _configChangeCallbacks;

    // Cache configuration in the base-class str
    bool _cacheConfig;

    // Non-volatile store empty
    bool _nonVolatileStoreEmpty;

    // Pointer to statically allocated storage which is used only when
    // non-volatile store is empty
    const char* _pStaticConfig;

    // Get non-volatile config str
    void getNVConfigStr(String& configStr) const;

    // Stats on calls to getNVConfigStr
    uint32_t _callsToGetNVStr;

};
