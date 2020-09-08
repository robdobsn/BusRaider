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
private:
    // Max length of config data
    int _configMaxDataLen;

    // Namespace used for Preferences lib
    String _configNamespace;

    // Preferences instance
    Preferences* _pPreferences;

    // List of callbacks on change of config
    std::vector<ConfigChangeCallbackType> _configChangeCallbacks;

public:
    ConfigNVS(const char *configNamespace, int configMaxlen);
    virtual ~ConfigNVS();

    // Clear
    virtual void clear() override final;

    // Initialise
    virtual bool setup() override final;

    // Write configuration string
    virtual bool writeConfig() override final;

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback) override final;

};
