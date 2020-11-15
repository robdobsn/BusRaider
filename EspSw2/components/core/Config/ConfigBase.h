/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigBase
// Base class of configuration classes
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <functional>
#include <RdJson.h>
#include <vector>
#include <WString.h>

typedef std::function<void()> ConfigChangeCallbackType;

class ConfigBase
{
public:
    ConfigBase()
    {
        _configMaxDataLen = 0;
    }

    ConfigBase(int maxDataLen) :
        _configMaxDataLen(maxDataLen)
    {
    }

    ConfigBase(const char* configStr)
    {
        _setConfigData(configStr);
    }

    ConfigBase(const String& configStr)
    {
        _setConfigData(configStr.c_str());
    }

    virtual ~ConfigBase()
    {
    }

    virtual void clear()
    {
    }

    virtual bool setup()
    {
        return false;
    }

    // Get config raw string
    virtual void getConfigString(String& configStr)
    {
        configStr = _dataStrJSON;
    }

    virtual bool writeConfig(const String& configJSONStr)
    {
        return false;
    }

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback)
    {
    }

    // Get max length
    int getMaxLen()
    {
        return _configMaxDataLen;
    }

    virtual String getString(const char *dataPath, const char *defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue, _dataStrJSON.c_str());
    }

    virtual String getString(const char *dataPath, const String& defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue.c_str(), _dataStrJSON.c_str());
    }

    virtual long getLong(const char *dataPath, long defaultValue)
    {
        return RdJson::getLong(dataPath, defaultValue, _dataStrJSON.c_str());
    }

    virtual double getDouble(const char *dataPath, double defaultValue)
    {
        return RdJson::getDouble(dataPath, defaultValue, _dataStrJSON.c_str());
    }

    virtual bool getArrayElems(const char *dataPath, std::vector<String>& strList)
    {
        return RdJson::getArrayElems(dataPath, strList, _dataStrJSON.c_str());
    }

    virtual bool contains(const char *dataPath)
    {
        int startPos = 0, strLen = 0;
        jsmntype_t elemType;
        int elemSize = 0;
        return RdJson::getElement(dataPath, startPos, strLen, elemType, elemSize, _dataStrJSON.c_str());
    }

    virtual bool getKeys(const char *dataPath, std::vector<String>& keysVector)
    {
        return RdJson::getKeys(dataPath, keysVector, _dataStrJSON.c_str());
    }

protected:
    // Set the configuration data directly
    virtual void _setConfigData(const char* configJSONStr)
    {
        if (strlen(configJSONStr) == 0)
            _dataStrJSON = "{}";
        else
            _dataStrJSON = configJSONStr;
    }

    // Data is stored in a single string as JSON
    String _dataStrJSON;

    // Max length of config data
    unsigned int _configMaxDataLen;
};
