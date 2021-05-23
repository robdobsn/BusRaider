// Config Pin Map (maps as string to a pin on a processor)
// Rob Dobson 2016-2021

#include <cstring>
#include <cstddef>
#include <cstdlib>
#include "ConfigPinMap.h"
#include "ConfigBase.h"
#include "Logger.h"
#include "ArduinoGPIO.h"

#define DEBUG_CONFIG_MULTIPLE

static const char* MODULE_PREFIX = "ConfigPinMap";

#if defined(ESP32) || defined(IDF_VER)
#if defined(ARDUINO_FEATHER_ESP32)
const char *ConfigPinMap::_pinMapOtherStr[] = {"DAC1", "DAC2", "SCL", "SDA", "RX", "TX", "MISO", "MOSI", "SCK"};
int ConfigPinMap::_pinMapOtherPin[] = {DAC1, DAC2, SCL, SDA, RX, TX, MISO, MOSI, SCK};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOtherPin) / sizeof(int);
int ConfigPinMap::_pinMapD[] = {};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#else
const char *ConfigPinMap::_pinMapOtherStr[] = {};
int ConfigPinMap::_pinMapOtherPin[] = {};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOtherPin) / sizeof(int);
int ConfigPinMap::_pinMapD[] = {};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#endif
#elif defined(ESP8266)

const char *ConfigPinMap::_pinMapOtherStr[] = {};
int ConfigPinMap::_pinMapOtherPin[] = {};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOtherPin) / sizeof(int);
int ConfigPinMap::_pinMapD[] = {16,5,4,0,2,14,12,13,15,3,1};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);


#else

const char *ConfigPinMap::_pinMapOtherStr[] = {};
int ConfigPinMap::_pinMapOtherPin[] = {};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOther) / sizeof(int);

#if PLATFORM_ID == 6    // Photon
int ConfigPinMap::_pinMapD[] = {D0, D1, D2, D3, D4, D5, D6, D7};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {A0, A1, A2, A3, A4, A5, A6, A7};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#elif PLATFORM_ID == 88 // RedBear Duo
int ConfigPinMap::_pinMapD[] = {D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15, D16, D17};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {A0, A1, A2, A3, A4, A5, A6, A7};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#else
int ConfigPinMap::_pinMapD[] = {0, 1, 2, 3, 4, 5, 6, 7};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {10, 11, 12, 13, 14, 15, 16, 17};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#endif

#endif

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
        return GPIO_INPUT_PULLUP;
#if !defined(ESP8266)
    if (strcasecmp(inputTypeStr, "INPUT_PULLDOWN") == 0)
        return GPIO_INPUT_PULLDOWN;
#endif
    return GPIO_INPUT;
}

// Configure multople IOs
void ConfigPinMap::configMultiple(ConfigBase& config, const char* pPinNames[], int pinModeArray[], 
            int pinInitial[], int* pinValues[], int numPins)
{
    for (int i = 0; i < numPins; i++)
    {
        String pinStr = config.getString(pPinNames[i], "");
        if (pinStr.length() == 0)
            return;
        int pinVal = ConfigPinMap::getPinFromName(pinStr.c_str());
        // Debug
#ifdef DEBUG_CONFIG_MULTIPLE
        LOG_I(MODULE_PREFIX, "setup pin name %s map %s num %d mode %d", 
                        pPinNames[i], pinStr.c_str(), pinVal, pinModeArray[i]);
#endif
        // Set the appropriate pins to output and initialise to 0
        if (pinModeArray[i] != GPIO_IOMODE_NONE)
        {
            pinMode(pinVal, pinModeArray[i]);
            if (pinModeArray[i] == OUTPUT)
                digitalWrite(pinVal, pinInitial[i]);
        }
        // Store accordingly
        *pinValues[i] = pinVal;
    }
}