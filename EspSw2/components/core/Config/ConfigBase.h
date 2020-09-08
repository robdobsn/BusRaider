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

typedef std::function<void()> ConfigChangeCallbackType;

class ConfigBase
{
protected:
    // Data is stored in a single string as JSON
    String _dataStrJSON;

    // Max length of config data
    unsigned int _configMaxDataLen;

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
        setConfigData(configStr);
    }

    ConfigBase(const String& configStr)
    {
        setConfigData(configStr.c_str());
    }

    virtual ~ConfigBase()
    {
    }

    // Get config data string
    virtual const char *c_str() const
    {
        return _dataStrJSON.c_str();
    }

    // Get reference to config WString
    virtual String& getConfigString()
    {
        return _dataStrJSON;
    }

    // Set the configuration data directly
    virtual void setConfigData(const char *configJSONStr)
    {
        if (strlen(configJSONStr) == 0)
            _dataStrJSON = "{}";
        else
            _dataStrJSON = configJSONStr;
    }

    virtual bool validateJson(int& numTokens)
    {
        return RdJson::validateJson(_dataStrJSON.c_str(), numTokens);
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

    virtual void clear()
    {
    }

    virtual bool setup()
    {
        return false;
    }

    virtual bool writeConfig()
    {
        return false;
    }

    // Register change callback
    virtual void registerChangeCallback(ConfigChangeCallbackType configChangeCallback)
    {
    }
};
