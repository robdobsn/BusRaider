/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigBase
// Base class of configuration classes
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <vector>

#include <RdJson.h>
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
    virtual String getConfigString() const
    {
        return _dataStrJSON;
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
    virtual int getMaxLen() const
    {
        return _configMaxDataLen;
    }

    /**
     * @brief Retrieve a JSON element as a string.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param dataPath     path of element to return
     * @param defaultValue default value to use if `dataPath` does not refer to
     *                     a value
     *
     * @return retrieved value or default
     */
    virtual String getString(const char *dataPath, const char *defaultValue) const;

    /**
     * @overload String ConfigBase::getString(const char *dataPath, const char *defaultValue)
     */
    virtual String getString(const char *dataPath, const String& defaultValue) const;

    /**
     * @brief Retrieve an integer value.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param dataPath     path of element to return
     * @param defaultValue default value to use if `dataPath` does not refer to
     *                     a value
     * @return `defaultValue` if `dataPath` does not refer to a value, zero if
     *         that value cannot be interpreted by strtol(), and the retrieved
     *         integer otherwise.
     */
    virtual long getLong(const char *dataPath, long defaultValue) const;

    /**
     * @brief Retrieve a decimal value.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param dataPath     path of element to return
     * @param defaultValue default value to use if `dataPath` does not refer to
     *                     a value
     * @return `defaultValue` if `dataPath` does not refer to a value, zero if
     *         that value cannot be interpreted by strtod(), and the retrieved
     *         number otherwise.
     */
    virtual double getDouble(const char *dataPath, double defaultValue) const;

    /**
     * @brief Retrieve elements of a JSON array if `dataPath` refers to one.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param[in]  dataPath   path of element to return
     * @param[out] arrayElems elems of array to return
     *
     * @return `true` if a JSON array was found at `dataPath`
     */
    virtual bool getArrayElems(const char *dataPath, std::vector<String>& strList) const;

    /**
     * @brief Check if `dataPath` refers to a value in this config.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param[in] dataPath path of element to check
     * @return `true` if this config has a value for `dataPath`
     */
    virtual bool contains(const char *dataPath) const;

    /**
     * @brief Retrieve keys of a JSON object if `dataPath` refers to one.
     *
     * Calls ConfigBase::getElement() to interpret `dataPath`.
     *
     * @param[in]  dataPath   path of element to return
     * @param[out] keysVector elems of array to return
     *
     * @return `true` if a JSON object was found at `dataPath`
     */
    virtual bool getKeys(const char *dataPath, std::vector<String>& keysVector) const;

    // Set hardware revision
    static void setHwRevision(int hwRevision)
    {
        _hwRevision = hwRevision;
    }
    
protected:
    /**
     * @brief Retrieve a JSON element specified by `dataPath`.
     *
     * If `dataPath` refers to a revision switch array, this is interpreted and
     * the value for the currrent HW revision is used.
     *
     * Revision switch arrays along the `dataPath` are not interpreted.
     *
     * @param[in]  dataPath    Path to an element in this config's internal JSON.
     * @param[out] elementStr  String representation of the JSON element found at
     *          `dataPath` (accounting for revision switching). Not modified if
     *          `dataPath` does not exist. Set to the revision switch array if
     *          there is one at `dataPath` but it does not contain a value for
     *          the current HW. May be set to the revision switch array or a
     *          default value if the revision switch array is corrupted.
     * @param[out] elementType Type of the retrieved element, if one was found.
     *          Not modified if `dataPath` does not exist. Invalid otherwise.
     * @param[in]  pConfigStr   Source string.
     *
     * @return `true` if `dataPath` exists and either does not point to a revision
     *         switch array or this array is valid and contains a value for the
     *         current HW revision (even if only the default value).
     */
    static bool _helperGetElement(const char *dataPath, String& elementStr, jsmntype_t& elementType, const char* pConfigStr);

    // Set the configuration data directly
    virtual void _setConfigData(const char* configJSONStr);

    // Helpers
    virtual const char* _getConfigCStr() const
    {
        return _dataStrJSON.c_str();
    }
    static String _helperGetString(const char *dataPath, const char *defaultValue, const char* sourceStr);
    static long _helperGetLong(const char *dataPath, long defaultValue, const char* pSourceStr);
    static double _helperGetDouble(const char *dataPath, double defaultValue, const char* pSourceStr);
    static bool _helperGetArrayElems(const char *dataPath, std::vector<String>& strList, const char* pSourceStr);
    static bool _helperGetKeys(const char *dataPath, std::vector<String>& keysVector, const char* pSourceStr);
    static bool _helperContains(const char *dataPath, const char* pSourceStr);

    // Data is stored in a single string as JSON
    String _dataStrJSON;

    // Max length of config data
    unsigned int _configMaxDataLen;

    // Hardware revision
    static const int DEFAULT_HARDWARE_REVISION_NUMBER = 1;
    static int _hwRevision;
};
