/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigMulti
// Configuration handling multiple underlying config objects in a hierarchy
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ConfigMulti.h"
#include "Logger.h"

// #define DEBUG_CONFIG_MULTI_DETAIL

// Logging
#ifdef DEBUG_CONFIG_MULTI_DETAIL
static const char* MODULE_PREFIX = "ConfigMulti";
#endif

ConfigMulti::ConfigMulti()
{
}

ConfigMulti::~ConfigMulti()
{
}

void ConfigMulti::addConfig(ConfigBase* pConfig, const char* prefix, bool isMutable)
{
    if (!pConfig)
        return;
    ConfigRec rec(pConfig, prefix, isMutable);
    _configsList.push_back(rec);
}

// Write configuration data to last (in order of adding) mutable element
bool ConfigMulti::writeConfig(const String& configJSONStr)
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

String ConfigMulti::getString(const char *dataPath, const char *defaultValue) const
{
    // Get base value
    String retVal = ConfigBase::getString(dataPath, defaultValue);

    // Iterate other configs in the order added
    for (const ConfigRec& rec : _configsList)
    {
        if (rec._prefix.length() > 0)
        {
            String prefixedPath = rec._prefix + "/" + String(dataPath);
            retVal = rec._pConfig->getString(prefixedPath.c_str(), retVal);
        }
        else
        {
            retVal = rec._pConfig->getString(dataPath, retVal);
        }
    }
    return retVal;
}

long ConfigMulti::getLong(const char *dataPath, long defaultValue) const
{
    // Get base value
    long retVal = ConfigBase::getLong(dataPath, defaultValue);

    // Iterate other configs in the order added
    for (const ConfigRec& rec : _configsList)
    {
        if (!rec._pConfig)
            continue;
        if (rec._prefix.length() > 0)
        {
            String prefixedPath = rec._prefix + "/" + String(dataPath);
            retVal = rec._pConfig->getLong(prefixedPath.c_str(), retVal);
#ifdef DEBUG_CONFIG_MULTI_DETAIL
            LOG_I(MODULE_PREFIX, "getLong prefix %s dataPath %s retVal %ld prefixedPath %s", rec._prefix.c_str(), dataPath, retVal, prefixedPath.c_str());
#endif
        }
        else
        {
            retVal = rec._pConfig->getLong(dataPath, retVal);
#ifdef DEBUG_CONFIG_MULTI_DETAIL
            LOG_I(MODULE_PREFIX, "getLong prefix %s dataPath %s retVal %ld", rec._prefix.c_str(), dataPath, retVal);
#endif
        }
    }
    return retVal;    
}

double ConfigMulti::getDouble(const char *dataPath, double defaultValue) const
{
    // Get base value
    double retVal = ConfigBase::getDouble(dataPath, defaultValue);

    // Iterate other configs in the order added
    for (const ConfigRec& rec : _configsList)
    {
        if (rec._prefix.length() > 0)
        {
            String prefixedPath = rec._prefix + "/" + String(dataPath);
            retVal = rec._pConfig->getDouble(prefixedPath.c_str(), retVal);
        }
        else
        {
            retVal = rec._pConfig->getDouble(dataPath, retVal);
        }
    }
    return retVal;    
}

bool ConfigMulti::getArrayElems(const char *dataPath, std::vector<String>& strList) const
{
    // Get base value
    bool retVal = ConfigBase::getArrayElems(dataPath, strList);

    // Iterate other configs in the order added
    for (const ConfigRec& rec : _configsList)
    {
        if (rec._prefix.length() > 0)
        {
            String prefixedPath = rec._prefix + "/" + String(dataPath);
            retVal |= rec._pConfig->getArrayElems(prefixedPath.c_str(), strList);
        }
        else
        {
            retVal |= rec._pConfig->getArrayElems(dataPath, strList);
        }
    }
    return retVal;    
}

// Register change callback
void ConfigMulti::registerChangeCallback(ConfigChangeCallbackType configChangeCallback)
{
    // Find last mutable element
    for (std::list<ConfigRec>::reverse_iterator rit=_configsList.rbegin(); rit != _configsList.rend(); ++rit)
    {
        if (rit->_isMutable)
            return rit->_pConfig->registerChangeCallback(configChangeCallback);
    }
}