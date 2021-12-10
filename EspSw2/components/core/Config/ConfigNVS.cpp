/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigNVS
// Configuration persisted to non-volatile storage
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ArPreferences.h>
#include <ConfigNVS.h>

// Logging
static const char* MODULE_PREFIX = "ConfigNVS";

// Debug
// #define DEBUG_NVS_CONFIG_READ_WRITE
// #define DEBUG_NVS_CONFIG_GETS
// #define DEBUG_NVS_CONFIG_DETAIL

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ConfigNVS::ConfigNVS(const char *configNamespace, int configMaxlen) :
    ConfigBase(configMaxlen)
{
    _configNamespace = configNamespace;
    _pPreferences = new ArPreferences();
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ConfigNVS::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Debug
    // LOG_D(MODULE_PREFIX "Config %s ...", _configNamespace.c_str());

    // Get string from non-volatile storage
    String configStr = getNVConfigStr();

    // Store value in config base class
    _setConfigData(configStr.c_str());

    // Check if we should use this config info in favour of
    // static pointer
    _nonVolatileStoreEmpty = (configStr.length() <= 2);

    // Ok
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write configuration string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ConfigNVS::writeConfig(const String& configJSONStr)
{
    // Check length of string
    if (configJSONStr.length() >= _configMaxDataLen)
    {
        LOG_E(MODULE_PREFIX, "writeConfig config too long %d > %d", configJSONStr.length(), _configMaxDataLen);
        return false;
    }

    // Set config data
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// registerChangeCallback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ConfigNVS::registerChangeCallback(ConfigChangeCallbackType configChangeCallback)
{
    // Save callback if required
    if (configChangeCallback)
    {
        _configChangeCallbacks.push_back(configChangeCallback);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getNVConfigStr
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ConfigNVS::getNVConfigStr() const
{
    if (!_pPreferences)
        return "";

    // Handle preferences bracketing - open read-only
    _pPreferences->begin(_configNamespace.c_str(), true);

    // Get config string
    String confStr = _pPreferences->getString("JSON", "{}");

#ifdef DEBUG_NVS_CONFIG_READ_WRITE
    // Debug
    LOG_W(MODULE_PREFIX, "getNVConfigStr %s read: len(%d) %s maxlen %d", 
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

    return confStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getString
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ConfigNVS::getString(const char *dataPath, const char *defaultValue) const
{
    // Check source
    if (_helperContains(dataPath, _dataStrJSON.c_str()))
        return ConfigBase::getString(dataPath, defaultValue);
    if (_pStaticConfig)
        return _helperGetString(dataPath, defaultValue, _pStaticConfig);
    return defaultValue;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getString
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

long ConfigNVS::getLong(const char *dataPath, long defaultValue) const
{
    // Check source
    if (_helperContains(dataPath, _dataStrJSON.c_str()))
    {
        long retVal = ConfigBase::getLong(dataPath, defaultValue);
#ifdef DEBUG_NVS_CONFIG_DETAIL
        LOG_I(MODULE_PREFIX, "getLong nvs contains %s retVal %ld", dataPath, retVal);
#endif
        return retVal;
    }
    if (_pStaticConfig)
    {
        long retVal = _helperGetLong(dataPath, defaultValue, _pStaticConfig);
#ifdef DEBUG_NVS_CONFIG_DETAIL
        LOG_I(MODULE_PREFIX, "getLong not in nvs .. from static %s retVal %ld", dataPath, retVal);
#endif
        return retVal;
    }
#ifdef DEBUG_NVS_CONFIG_DETAIL
    LOG_I(MODULE_PREFIX, "getLong default val ret for %s", dataPath);
#endif
    return defaultValue;
}

// Get double
double ConfigNVS::getDouble(const char *dataPath, double defaultValue) const
{
    // Check source
    if (_helperContains(dataPath, _dataStrJSON.c_str()))
        return ConfigBase::getDouble(dataPath, defaultValue);
    if (_pStaticConfig)
        return _helperGetDouble(dataPath, defaultValue, _pStaticConfig);
    return defaultValue;
}

bool ConfigNVS::getArrayElems(const char *dataPath, std::vector<String>& strList) const
{
    // Check source
    if (_helperContains(dataPath, _dataStrJSON.c_str()))
        return ConfigBase::getArrayElems(dataPath, strList);
    if (_pStaticConfig)
        return _helperGetArrayElems(dataPath, strList, _pStaticConfig);
    return false;
}

bool ConfigNVS::contains(const char *dataPath) const
{
    // Check source
    if (_helperContains(dataPath, _dataStrJSON.c_str()))
        return true;
    if (_pStaticConfig)
        return _helperContains(dataPath, _pStaticConfig);
    return false;
}
