/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigMulti
// Configuration handling multiple underlying config objects in a hierarchy
//
// Rob Dobson 2020
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

    ConfigMulti()
    {
    }

    virtual ~ConfigMulti()
    {
    }

    void addConfig(ConfigBase* pConfig, const char* prefix, bool isMutable)
    {
        if (!pConfig)
            return;
        ConfigRec rec(pConfig, prefix, isMutable);
        _configsList.push_back(rec);
    }

    // Write configuration data to last (in order of adding) mutable element
    virtual bool writeConfig(const String& configJSONStr) override final
    {
        // Find last mutable element
        for (std::list<ConfigRec>::reverse_iterator rit=_configsList.rbegin(); rit != _configsList.rend(); ++rit)
        {
            if (rit->_isMutable)
                return rit->_pConfig->writeConfig(configJSONStr);
        }

        // Write base if not already done
        return ConfigBase::writeConfig(configJSONStr);
    }

    virtual String getString(const char *dataPath, const char *defaultValue) override final
    {
        // Get base value
        String retVal = ConfigBase::getString(dataPath, defaultValue);

        // Iterate other configs in the order added
        for (ConfigRec& rec : _configsList)
        {
            if (rec._prefix.length() > 0)
            {
                ConfigBase conf = rec._pConfig->getString(rec._prefix.c_str(), "{}");
                retVal = conf.getString(dataPath, retVal);
            }
            else
            {
                retVal = rec._pConfig->getString(dataPath, retVal);
            }
        }
        return retVal;
    }

    virtual String getString(const char *dataPath, const String& defaultValue) override final
    {
        // Get base value
        String retVal = ConfigBase::getString(dataPath, defaultValue);

        // Iterate other configs in the order added
        for (ConfigRec& rec : _configsList)
        {
            if (rec._prefix.length() > 0)
            {
                ConfigBase conf = rec._pConfig->getString(rec._prefix.c_str(), "{}");
                retVal = conf.getString(dataPath, retVal);
            }
            else
            {
                retVal = rec._pConfig->getString(dataPath, retVal);
            }
        }
        return retVal;    
    }

    virtual long getLong(const char *dataPath, long defaultValue) override final
    {
        // Get base value
        long retVal = ConfigBase::getLong(dataPath, defaultValue);

        // Iterate other configs in the order added
        for (ConfigRec& rec : _configsList)
        {
            if (!rec._pConfig)
                continue;
            if (rec._prefix.length() > 0)
            {
                ConfigBase conf = rec._pConfig->getString(rec._prefix.c_str(), "{}");
                retVal = conf.getLong(dataPath, retVal);
            }
            else
            {
                retVal = rec._pConfig->getLong(dataPath, retVal);
            }
        }
        return retVal;    
    }

    virtual double getDouble(const char *dataPath, double defaultValue) override final
    {
        // Get base value
        double retVal = ConfigBase::getDouble(dataPath, defaultValue);

        // Iterate other configs in the order added
        for (ConfigRec& rec : _configsList)
        {
            if (rec._prefix.length() > 0)
            {
                ConfigBase conf = rec._pConfig->getString(rec._prefix.c_str(), "{}");
                retVal = conf.getDouble(dataPath, retVal);
            }
            else
            {
                retVal = rec._pConfig->getDouble(dataPath, retVal);
            }
        }
        return retVal;    
    }

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback) override final
    {
        // Find last mutable element
        for (std::list<ConfigRec>::reverse_iterator rit=_configsList.rbegin(); rit != _configsList.rend(); ++rit)
        {
            if (rit->_isMutable)
                return rit->_pConfig->registerChangeCallback(configChangeCallback);
        }
    }

private:
    std::list<ConfigRec> _configsList;
  
};
