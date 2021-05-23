/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigNVS
// Configuration persisted to non-volatile storage
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <RdPreferences.h>
#include <ConfigNVS.h>

// Logging
static const char* MODULE_PREFIX = "ConfigNVS";

// Debug
// #define DEBUG_NVS_CONFIG_READ_WRITE
// #define DEBUG_NVS_CONFIG_GETS

ConfigNVS::ConfigNVS(const char *configNamespace, int configMaxlen, bool cacheConfig) :
    ConfigBase(configMaxlen)
{
    _configNamespace = configNamespace;
    _pPreferences = new RdPreferences();
    _cacheConfig = cacheConfig;
    _nonVolatileStoreEmpty = true;
    _pStaticConfig = NULL;
    _callsToGetNVStr = 0;
    setup();
}

ConfigNVS::~ConfigNVS()
{
    if (_pPreferences)
        delete _pPreferences;
}

// Clear
void ConfigNVS::clear()
{
    // Setup preferences
    if (_pPreferences)
    {
        // Open preferences writeable
        _pPreferences->begin(_configNamespace.c_str(), false);

        // Clear namespace
        _pPreferences->clear();

        // Close prefs
        _pPreferences->end();
    }

    // Set the config str to empty
    ConfigBase::clear();

    // Clear the config string
    _setConfigData("");

    // No non-volatile store
    _nonVolatileStoreEmpty = true;
}

// Initialise
bool ConfigNVS::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Debug
    // LOG_D(MODULE_PREFIX "Config %s ...", _configNamespace.c_str());

    // Get string from non-volatile storage
    String configStr;
    getNVConfigStr(configStr);

    // Check if config string to be cached in config base class
    if (_cacheConfig)
        _setConfigData(configStr.c_str());

    // Check if we should use this config info in favour of
    // static pointer
    _nonVolatileStoreEmpty = (configStr.length() <= 2);

    // Ok
    return true;
}

// Write configuration string
bool ConfigNVS::writeConfig(const String& configJSONStr)
{
    // Check length of string
    if (configJSONStr.length() >= _configMaxDataLen)
    {
        LOG_E(MODULE_PREFIX, "writeConfig config too long %d > %d", configJSONStr.length(), _configMaxDataLen);
        return false;
    }

    // Set config data if cached
    if (_cacheConfig)
        _setConfigData(configJSONStr.c_str());

    // Check if non-volatile data is valid
    _nonVolatileStoreEmpty = (configJSONStr.length() <= 2);

#ifdef DEBUG_NVS_CONFIG_READ_WRITE
    // Debug
    LOG_W(MODULE_PREFIX, "writeConfig %s config len: %d", 
                _configNamespace.c_str(), configJSONStr.length());
#endif

    // Handle preferences bracketing
    if (_pPreferences)
    {
        // Open preferences writeable
        _pPreferences->begin(_configNamespace.c_str(), false);

        // Set config string
        int numPut = _pPreferences->putString("JSON", configJSONStr.c_str());
        if (numPut != configJSONStr.length())
        {
            LOG_E(MODULE_PREFIX, "writeConfig Writing Failed %s written = %d", 
                _configNamespace.c_str(), numPut);
        }
        else
        {
            // LOG_D(MODULE_PREFIX, "Write ok written = %d", numPut);
        }

        // Close prefs
        _pPreferences->end();
    }

    // Call config change callbacks
    for (int i = 0; i < _configChangeCallbacks.size(); i++)
    {
        if (_configChangeCallbacks[i])
        {
            (_configChangeCallbacks[i])();
        }
    }

    // Ok
    return true;
}

void ConfigNVS::registerChangeCallback(ConfigChangeCallbackType configChangeCallback)
{
    // Save callback if required
    if (configChangeCallback)
    {
        _configChangeCallbacks.push_back(configChangeCallback);
    }
}

void ConfigNVS::getNVConfigStr(String& confStr) const
{
    if (!_pPreferences)
        return;

    // Handle preferences bracketing - open read-only
    _pPreferences->begin(_configNamespace.c_str(), true);

    // Get config string
    confStr = _pPreferences->getString("JSON", "{}");

#ifdef DEBUG_NVS_CONFIG_READ_WRITE
    // Debug
    LOG_W(MODULE_PREFIX, "Config %s read: len(%d) %s maxlen %d", 
                _configNamespace.c_str(), confStr.length(), 
                confStr.c_str(), ConfigBase::getMaxLen());
#endif

    // Close prefs
    _pPreferences->end();

    // Stats
    const_cast<ConfigNVS*>(this)->_callsToGetNVStr++;

#ifdef DEBUG_NVS_CONFIG_GETS
    LOG_I(MODULE_PREFIX, "getNVConfigStr count %d", _callsToGetNVStr);
#endif

}
