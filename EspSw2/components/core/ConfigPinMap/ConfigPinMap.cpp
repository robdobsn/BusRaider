// Config Pin Map (maps as string to a pin on a processor)
// Rob Dobson 2016-2021

#include <cstring>
#include <cstddef>
#include <cstdlib>
#include "ConfigPinMap.h"
#include "ConfigBase.h"
#include "Logger.h"
#include "ArduinoGPIO.h"

// #define DEBUG_CONFIG_MULTIPLE

#ifdef DEBUG_CONFIG_MULTIPLE
static const char* MODULE_PREFIX = "ConfigPinMap";
#endif

#if defined(ESP32) || defined(IDF_VER)
#if defined(ARDUINO_FEATHER_ESP32)
#define CONFIG_PINMAP_INCLUDES_DIGITAL
#define CONFIG_PINMAP_INCLUDES_ANALOG
#define CONFIG_PINMAP_INCLUDES_OTHER
const char *ConfigPinMap::_pinMapOtherStr[] = {"DAC1", "DAC2", "SCL", "SDA", "RX", "TX", "MISO", "MOSI", "SCK"};
int ConfigPinMap::_pinMapOtherPin[] = {DAC1, DAC2, SCL, SDA, RX, TX, MISO, MOSI, SCK};
int ConfigPinMap::_pinMapOtherLen = sizeof(ConfigPinMap::_pinMapOtherPin) / sizeof(int);
int ConfigPinMap::_pinMapD[] = {};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
int ConfigPinMap::_pinMapA[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12};
int ConfigPinMap::_pinMapALen = sizeof(ConfigPinMap::_pinMapA) / sizeof(int);
#endif
#elif defined(ESP8266)
#define CONFIG_PINMAP_INCLUDES_DIGITAL
int ConfigPinMap::_pinMapD[] = {16,5,4,0,2,14,12,13,15,3,1};
int ConfigPinMap::_pinMapDLen = sizeof(ConfigPinMap::_pinMapD) / sizeof(int);
#endif

int ConfigPinMap::getPinFromName(const char *pinName)
{
#ifdef CONFIG_PINMAP_INCLUDES_OTHER
    // Check "other" for full match to pin name
    for (int i = 0; i < _pinMapOtherLen; i++)
    {
        if (strcmp(_pinMapOtherStr[i], pinName) == 0)
            return _pinMapOtherPin[i];
    }
#endif
    // Check for D# and A# pins
    switch (*pinName)
    {
#ifdef CONFIG_PINMAP_INCLUDES_DIGITAL
        case 'D':
        {
            int pinIdx = (int)strtol(pinName + 1, NULL, 10);
            if (pinIdx >= 0 && pinIdx < _pinMapDLen)
                return _pinMapD[pinIdx];
            return -1;
        }
#endif
#ifdef CONFIG_PINMAP_INCLUDES_ANALOG
        case 'A':
        {
            int pinIdx = (int)strtol(pinName + 1, NULL, 10);
            if (pinIdx >= 0 && pinIdx < _pinMapALen)
                return _pinMapA[pinIdx];
            return -1;
        }
#endif
        default:
            if (strlen(pinName) == 0)
                return -1;
            return (int)strtol(pinName, NULL, 10);
    }
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

int ConfigPinMap::mapPinModeToArduino(PinMode pinMode)
{
    switch (pinMode)
    {
        case GPIO_OUTPUT:
            return OUTPUT;
        case GPIO_INPUT:
            return INPUT;
        case GPIO_INPUT_PULLUP:
            return INPUT_PULLUP;
        case GPIO_INPUT_PULLDOWN:
            return INPUT_PULLDOWN;
        default:
            return INPUT;
    }
}

// Configure multople IOs
void ConfigPinMap::configMultiple(ConfigBase& config, PinDef* pPinDefs, int numPinDefs, bool deinit)
{
    // Loop through pins
    for (int defIdx = 0; defIdx < numPinDefs; defIdx++)
    {
        // Set invalid initially
        pPinDefs[defIdx]._pinNumber = -1;
        if (pPinDefs[defIdx]._pPinNumber)
            *pPinDefs[defIdx]._pPinNumber = -1;
        
        // Get pin name and number if valid
        String pinStr = config.getString(pPinDefs[defIdx]._name.c_str(), "");
        if (pinStr.length() == 0)
            continue;
        int pinNumber = ConfigPinMap::getPinFromName(pinStr.c_str());
        if (pinNumber < 0)
            continue;

        // Debug
#ifdef DEBUG_CONFIG_MULTIPLE
        LOG_I(MODULE_PREFIX, "setup pin name %s map %s num %d mode %d deinit %d", 
                        pPinDefs[defIdx]._name.c_str(), pinStr.c_str(), pinNumber,
                        pPinDefs[defIdx]._pinMode, deinit);
#endif

        // Check for deinit
        if (deinit)
        {
            pinMode(pinNumber, INPUT);
        }
        else
        {
            // Get mapped pin mode
            int mappedPinMode = mapPinModeToArduino(pPinDefs[defIdx]._pinMode);
            pinMode(pinNumber, mappedPinMode);
            if (pPinDefs[defIdx]._pinMode == GPIO_OUTPUT)
                digitalWrite(pinNumber, pPinDefs[defIdx]._initialLevel);
        }
        // Store accordingly
        pPinDefs[defIdx]._pinNumber = pinNumber;
        if (pPinDefs[defIdx]._pPinNumber != NULL)
            *(pPinDefs[defIdx]._pPinNumber) = pinNumber;
    }
}
