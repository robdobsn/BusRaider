/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigNVS
// Configuration persisted to non-volatile storage
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <Preferences.h>
#include <ConfigNVS.h>

// Logging
static const char* MODULE_PREFIX = "ConfigNVS";

// Debug
// #define DEBUG_NVS_CONFIG_READ_WRITE

ConfigNVS::ConfigNVS(const char *configNamespace, int configMaxlen) :
    ConfigBase(configMaxlen)
{
    _configNamespace = configNamespace;
    _pPreferences = new Preferences();
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
    setConfigData("");
}

// Initialise
bool ConfigNVS::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Debug
    // LOG_D(MODULE_PREFIX "Config %s ...", _configNamespace.c_str());

    // Handle preferences bracketing
    if (_pPreferences)
    {
        // Open preferences read-only
        _pPreferences->begin(_configNamespace.c_str(), true);

        // Get config string
        String configData = _pPreferences->getString("JSON", "{}");
        setConfigData(configData.c_str());

#ifdef DEBUG_NVS_CONFIG_READ_WRITE
        // Debug
        LOG_W(MODULE_PREFIX, "Config %s read: len(%d) %s maxlen %d", 
                    _configNamespace.c_str(), configData.length(), 
                    configData.c_str(), ConfigBase::getMaxLen());
#endif

        // Close prefs
        _pPreferences->end();
    }

    // Ok
    return true;
}

// Write configuration string
bool ConfigNVS::writeConfig()
{
    // Get length of string
    if (_dataStrJSON.length() >= _configMaxDataLen)
        _dataStrJSON = _dataStrJSON.substring(0, _configMaxDataLen-1);

#ifdef DEBUG_NVS_CONFIG_READ_WRITE
    // Debug
    LOG_W(MODULE_PREFIX, "writeConfig %s config len: %d", 
                _configNamespace.c_str(), _dataStrJSON.length());
#endif

    // Handle preferences bracketing
    if (_pPreferences)
    {
        // Open preferences writeable
        _pPreferences->begin(_configNamespace.c_str(), false);

        // Set config string
        int numPut = _pPreferences->putString("JSON", _dataStrJSON.c_str());
        if (numPut != _dataStrJSON.length())
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
