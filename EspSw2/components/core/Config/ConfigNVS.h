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
class Preferences;

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
    virtual void getConfigString(String& configStr) override final
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

    virtual String getString(const char *dataPath, const char *defaultValue) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getString(dataPath, defaultValue, configStr.c_str());
    }

    virtual String getString(const char *dataPath, const String& defaultValue) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getString(dataPath, defaultValue.c_str(), configStr.c_str());
    }

    virtual long getLong(const char *dataPath, long defaultValue) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getLong(dataPath, defaultValue, configStr.c_str());
    }

    virtual double getDouble(const char *dataPath, double defaultValue) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getDouble(dataPath, defaultValue, configStr.c_str());
    }

    virtual bool getArrayElems(const char *dataPath, std::vector<String>& strList) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getArrayElems(dataPath, strList, configStr.c_str());
    }

    virtual bool contains(const char *dataPath) override final
    {
        String configStr;
        getConfigString(configStr);
        int startPos = 0, strLen = 0;
        jsmntype_t elemType;
        int elemSize = 0;
        return RdJson::getElement(dataPath, startPos, strLen, elemType, elemSize, configStr.c_str());
    }

    virtual bool getKeys(const char *dataPath, std::vector<String>& keysVector) override final
    {
        String configStr;
        getConfigString(configStr);
        return RdJson::getKeys(dataPath, keysVector, configStr.c_str());
    }

private:
    // Namespace used for Preferences lib
    String _configNamespace;

    // Preferences instance
    Preferences* _pPreferences;

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
    void getNVConfigStr(String& configStr);

    // Stats on calls to getNVConfigStr
    uint32_t _callsToGetNVStr;

};
