/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigMulti
// Configuration handling multiple underlying config objects in a hierarchy
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <list>

class ConfigMulti : public ConfigBase
{
public:

    class ConfigRec
    {
        public:
            ConfigRec()
            {
                _pConfig = NULL;
                _isMutable = false;
            }
            ConfigRec(const ConfigRec& other)
            {
                _pConfig = other._pConfig;
                _isMutable = other._isMutable;
                _prefix = other._prefix;
            }
            ConfigRec(ConfigBase* pConfig, const char* prefix, bool isMutable)
            {
                _pConfig = pConfig;
                _prefix = prefix;
                _isMutable = isMutable;
            }
            ConfigBase* _pConfig;
            String _prefix;
            bool _isMutable;
    };

    ConfigMulti();
    virtual ~ConfigMulti();
    void addConfig(ConfigBase* pConfig, const char* prefix, bool isMutable);

    // Write configuration data to last (in order of adding) mutable element
    virtual bool writeConfig(const String& configJSONStr) override final;

    // Get functions
    virtual String getString(const char *dataPath, const char *defaultValue) const override final;
    virtual long getLong(const char *dataPath, long defaultValue) const override final;
    virtual double getDouble(const char *dataPath, double defaultValue) const override final;
    virtual bool getArrayElems(const char *dataPath, std::vector<String>& strList) const override final;

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback) override final;

private:
    std::list<ConfigRec> _configsList;
  
};
