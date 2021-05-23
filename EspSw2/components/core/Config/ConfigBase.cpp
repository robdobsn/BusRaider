/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigBase
// Base class of configuration classes
//
// Branislav Pilnan 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ConfigBase.h"

#include <ESPUtils.h>
#include <Logger.h>

// #define DEBUG_CHECK_BACKWARDS_COMPATIBILITY
// #define DEBUG_CONFIG_BASE
// #define DEBUG_CONFIG_BASE_SPECIFIC_PATH "numpix"

// Logging
const char* MODULE_PREFIX = "ConfigBase";

bool ConfigBase::getElement(const char *dataPath, String& elementStr, jsmntype_t& elementType) const
{
#ifdef DEBUG_CONFIG_BASE
    LOG_I(MODULE_PREFIX, "Looking for element '%s'", dataPath);
#endif

    String configStr;
    getConfigString(configStr);

    int startPos = -1;
    int strLen = -1;
    int size = -1;
    bool found = RdJson::getElement(dataPath, startPos, strLen, elementType, size, configStr.c_str());
#ifdef DEBUG_CONFIG_BASE
    LOG_I(MODULE_PREFIX, "  - found=%d, start=%d, len=%d, type=%s, size=%d, elemSubstr='%s'",
            (int)found, startPos, strLen, RdJson::getElemTypeStr(elementType), size,
            (startPos < 0 || strLen < 0) ? "" : configStr.substring(startPos, startPos + strLen).c_str());
#endif
#ifdef DEBUG_CONFIG_BASE_SPECIFIC_PATH
    if (strcasecmp(dataPath, "numpix") == 0)
    {
        LOG_I(MODULE_PREFIX, "path %s found=%d, start=%d, len=%d, type=%s, size=%d, elemSubstr='%s'",
            dataPath, (int)found, startPos, strLen, RdJson::getElemTypeStr(elementType), size,
            (startPos < 0 || strLen < 0) ? "" : configStr.substring(startPos, startPos + strLen).c_str());
    }
#endif

    if (!found)
        return false;  // Invalid path

    elementStr = configStr.substring(startPos, startPos + strLen);
    if (elementType != JSMN_ARRAY || size <= 0)
        return true;  // Not an array of option objects - return the element as is

    // Try to parse an array of option objects
    std::vector<String> optionObjs(size);
    RdJson::getArrayElems("", optionObjs, elementStr.c_str());
    bool hasSomeOptionObjs = false;
    bool hasOnlyOptionObjs = true;
    jsmntype_t defaultValueType = JSMN_UNDEFINED;
    for (auto &&optionObj : optionObjs)
    {
        optionObj.trim();
        if (!optionObj.startsWith("{"))
            break;
        std::vector<String> hwRevs;
        bool hasHwRevs = RdJson::getArrayElems("__hwRevs__", hwRevs, optionObj.c_str());
        bool hasValue = RdJson::getElement("__value__", startPos, strLen, elementType, size, optionObj.c_str());
        bool isOptionObj = hasHwRevs && hasValue;
        hasSomeOptionObjs = hasSomeOptionObjs || isOptionObj;
        hasOnlyOptionObjs = hasOnlyOptionObjs && isOptionObj;
        if (!isOptionObj)
            break;

        // We have an option object - process it
        if (hwRevs.size() == 0)
        {
            // We found a default value - store it if there was no better match
            defaultValueType = elementType;
            elementStr = optionObj.substring(startPos, startPos + strLen);
            continue;
        }

        // Check if the option object applies to the current HW revision
        for (auto &&hwRevJson : hwRevs)
        {
            long hwRev = strtol(hwRevJson.c_str(), nullptr, 0);
            if (hwRev == getHwRevision())
            {
                // Found a value specifically for the current HW revision - return it
                elementStr = optionObj.substring(startPos, startPos + strLen);
                return true;
            }
        }
    }

    // No value found for the current hardware revision
    // There might still be a default value or the array may not have been a revision switch
    if (hasSomeOptionObjs && !hasOnlyOptionObjs)
    {
        LOG_E(MODULE_PREFIX, "Corrupted revision switch array '%s'", elementStr.c_str());
        return false;
    }
    else if (!hasSomeOptionObjs)
    {
        // The first element was not an option object - assume a normal array, not a revision switch
        return true;
    }
    else if (defaultValueType != JSMN_UNDEFINED)
    {
        // We checked the whole array and found a default value. There are only valid option objects.
        elementType = defaultValueType;
        return true;
    }
    else
    {
        return false;  // No value for this revision
    }
}

String ConfigBase::getString(const char *dataPath, const char *defaultValue) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    bool exists = getElement(dataPath, element, type);

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    String configStr;
    getConfigString(configStr);
    String rdJsonResult = RdJson::getString(dataPath, defaultValue, configStr.c_str());
    if (!exists && !rdJsonResult.equals(defaultValue))
    {
        // We are returning defaultValue when we should not
        LOG_W(MODULE_PREFIX, "getString(\"%s\", \"%s\") returned '%s' != '%s'",
              dataPath, defaultValue, defaultValue, rdJsonResult.c_str());
    }
#endif

    if (!exists)
        return defaultValue;

    // Normalise whitespace
    char *pStr = RdJson::safeStringDup(element.c_str(), element.length(),
                                       !(type == JSMN_STRING || type == JSMN_PRIMITIVE));
    if (pStr)
    {
        element = pStr;
        delete[] pStr;
    }

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    if (!rdJsonResult.equals(element))
    {
        LOG_W(MODULE_PREFIX, "getString(\"%s\", \"%s\") returned '%s' != '%s'",
              dataPath, defaultValue, element.c_str(), rdJsonResult.c_str());
    }
#endif

    return element;
}

String ConfigBase::getString(const char *dataPath, const String& defaultValue) const
{
    return getString(dataPath, defaultValue.c_str());
}

long ConfigBase::getLong(const char *dataPath, long defaultValue) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    bool exists = getElement(dataPath, element, type);
    long result = exists ? strtol(element.c_str(), nullptr, 0) : defaultValue;

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    String configStr;
    getConfigString(configStr);
    long rdJsonResult = RdJson::getLong(dataPath, defaultValue, configStr.c_str());
    if (rdJsonResult != result)
    {
        LOG_W(MODULE_PREFIX, "getLong(\"%s\", %ld) returned %ld != %ld",
              dataPath, defaultValue, result, rdJsonResult);
    }
#endif

    return result;
}

double ConfigBase::getDouble(const char *dataPath, double defaultValue) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    bool exists = getElement(dataPath, element, type);
    double result = exists ? strtod(element.c_str(), nullptr) : defaultValue;

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    String configStr;
    getConfigString(configStr);
    double rdJsonResult = RdJson::getDouble(dataPath, defaultValue, configStr.c_str());
    if (rdJsonResult != result)
    {
        LOG_W(MODULE_PREFIX, "getDouble(\"%s\", %f) returned %f != %f",
              dataPath, defaultValue, result, rdJsonResult);
    }
#endif

    return result;
}

bool ConfigBase::getArrayElems(const char *dataPath, std::vector<String>& strList) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    bool exists = getElement(dataPath, element, type);

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    String configStr;
    getConfigString(configStr);
    std::vector<String> rdJsonResult;
    double rdJsonExists = RdJson::getArrayElems(dataPath, rdJsonResult, configStr.c_str());
    if (!exists && rdJsonExists)
    {
        LOG_W(MODULE_PREFIX, "getArrayElems(\"%s\") - getElement() failed", dataPath);
    }
#endif

    if (!exists)
        return false;

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    bool result = RdJson::getArrayElems("", strList, element.c_str());
    if (result != rdJsonExists)
    {
        LOG_W(MODULE_PREFIX, "getArrayElems(\"%s\") == %d but RdJson::getArrayElems(\"%s\") == %d",
              dataPath, (int)result, dataPath, (int)rdJsonExists);
    }
    if (strList.size() != rdJsonResult.size())
    {
        LOG_W(MODULE_PREFIX, "getArrayElems(\"%s\") found %d!=%d array elems",
              dataPath, strList.size(), rdJsonResult.size());
    }
    for (int i = 0; i < strList.size() && i < rdJsonResult.size(); ++i)
    {
        if (!strList[i].equals(rdJsonResult[i]))
        {
            LOG_W(MODULE_PREFIX, "getArrayElems(\"%s\"): elem %d mismatch '%s' != '%s'",
              dataPath, i, strList[i].c_str(), rdJsonResult[i].c_str());
            break;
        }
    }
    return result;
#endif

    return RdJson::getArrayElems("", strList, element.c_str());
}

bool ConfigBase::contains(const char *dataPath) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    return getElement(dataPath, element, type);
}

bool ConfigBase::getKeys(const char *dataPath, std::vector<String>& keysVector) const
{
    String element;
    jsmntype_t type = JSMN_UNDEFINED;
    bool exists = getElement(dataPath, element, type);

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    String configStr;
    getConfigString(configStr);
    std::vector<String> rdJsonResult;
    double rdJsonExists = RdJson::getKeys(dataPath, rdJsonResult, configStr.c_str());
    if (!exists && rdJsonExists)
    {
        LOG_W(MODULE_PREFIX, "getKeys(\"%s\") - getElement() failed", dataPath);
    }
#endif

    if (!exists)
        return false;

#ifdef DEBUG_CHECK_BACKWARDS_COMPATIBILITY
    bool result = RdJson::getKeys("", keysVector, element.c_str());
    if (result != rdJsonExists)
    {
        LOG_W(MODULE_PREFIX, "getKeys(\"%s\") == %d but RdJson::getKeys(\"%s\") == %d",
              dataPath, (int)result, dataPath, (int)rdJsonExists);
    }
    if (keysVector.size() != rdJsonResult.size())
    {
        LOG_W(MODULE_PREFIX, "getKeys(\"%s\") found %d!=%d keys",
              dataPath, keysVector.size(), rdJsonResult.size());
    }
    for (int i = 0; i < keysVector.size() && i < rdJsonResult.size(); ++i)
    {
        if (!keysVector[i].equals(rdJsonResult[i]))
        {
            LOG_W(MODULE_PREFIX, "getKeys(\"%s\"): key %d mismatch '%s' != '%s'",
              dataPath, i, keysVector[i].c_str(), rdJsonResult[i].c_str());
            break;
        }
    }
    return result;
#endif

    return RdJson::getKeys("", keysVector, element.c_str());
}

void ConfigBase::_setConfigData(const char* configJSONStr)
{
    if (strlen(configJSONStr) == 0)
        _dataStrJSON = "{}";
    else
        _dataStrJSON = configJSONStr;
}
