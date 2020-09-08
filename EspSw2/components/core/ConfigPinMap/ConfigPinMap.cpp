// Config Pin Map (maps as string to a pin on a processor)
// Rob Dobson 2016-2018

#include <cstring>
#include <cstddef>
#include <cstdlib>
#include "ConfigPinMap.h"

const char *ConfigPinMap::_pinMapOtherStr[] = {};
int ConfigPinMap::_pinMapOtherPin[] = {};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOtherPin) / sizeof(int);
int ConfigPinMap::_pinMapD[] = {};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);

int ConfigPinMap::getPinFromName(const char *pinName)
{
    // Check "other" for full match to pin name
    for (int i = 0; i < _pinMapOtherLen; i++)
    {
        if (strcmp(_pinMapOtherStr[i], pinName) == 0)
            return _pinMapOtherPin[i];
    }
    // Check for D# and A# pins
    switch (*pinName)
    {
    case 'D':
    {
        int pinIdx = (int)strtol(pinName + 1, NULL, 10);
        if (pinIdx >= 0 && pinIdx < _pinMapDLen)
            return _pinMapD[pinIdx];
        return -1;
    }
    case 'A':
    {
        int pinIdx = (int)strtol(pinName + 1, NULL, 10);
        if (pinIdx >= 0 && pinIdx < _pinMapALen)
            return _pinMapA[pinIdx];
        return -1;
    }
    }
    if (strlen(pinName) == 0)
        return -1;
    return (int)strtol(pinName, NULL, 10);
}

// Conversion from strings like:
// PULLUP, PULLDOWN to INPUT_PULLUP, etc
int ConfigPinMap::getInputType(const char *inputTypeStr)
{
    if (strcasecmp(inputTypeStr, "INPUT_PULLUP") == 0)
        return INPUT_PULLUP;
#if !defined(ESP8266)
    if (strcasecmp(inputTypeStr, "INPUT_PULLDOWN") == 0)
        return INPUT_PULLDOWN;
#endif
    return INPUT;
}