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
class ArPreferences;

class ConfigNVS : public ConfigBase
{
public:
    ConfigNVS(const char* configNamespace, int configMaxlen);
    virtual ~ConfigNVS();

    // Set the config data from a static source - note that only the
    // pointer is stored so this data MUST be statically allocated
    void setStaticConfigData(const char* pStaticConfig)
    {
        _pStaticConfig = pStaticConfig;
    }

    // Get config raw string
    virtual String getConfigString() const override final
    {
        return (_nonVolatileStoreEmpty && _pStaticConfig) ? _pStaticConfig : _dataStrJSON;
    }

    // Get persisted config
    String getPersistedConfig() const
    {
        if (!_nonVolatileStoreEmpty)
            if (_dataStrJSON.length() > 0)
                return _dataStrJSON;
        return "{}";
    }

    // Get static config
    String getStaticConfig() const
    {
        if (_pStaticConfig)
            return _pStaticConfig;
        return "{}";
    }

    // Clear
    virtual void clear() override final;

    // Initialise
    virtual bool setup() override final;

    // Write configuration string
    virtual bool writeConfig(const String& configJSONStr) override final;

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback) override final;

    // Get string
    virtual String getString(const char *dataPath, const char *defaultValue) const override final;

    // Get long
    virtual long getLong(const char *dataPath, long defaultValue) const override final;

    // Get double
    virtual double getDouble(const char *dataPath, double defaultValue) const override final;

    // Get array elems
    virtual bool getArrayElems(const char *dataPath, std::vector<String>& strList) const override final;

    // Check if config contains key
    virtual bool contains(const char *dataPath) const override final;

private:
    // Namespace used for ArPreferences lib
    String _configNamespace;

    // ArPreferences instance
    ArPreferences* _pPreferences;

    // List of callbacks on change of config
    std::vector<ConfigChangeCallbackType> _configChangeCallbacks;

    // Non-volatile store empty
    bool _nonVolatileStoreEmpty;

    // Pointer to statically allocated storage which is used only when
    // non-volatile store is empty
    const char* _pStaticConfig;

    // Get non-volatile config str
    String getNVConfigStr() const;

    // Stats on calls to getNVConfigStr
    uint32_t _callsToGetNVStr;
};
