// Config Pin Map (maps as string to a pin on a processor)
// Rob Dobson 2016-2021

#pragma once

#include "WString.h"

class ConfigBase;

class ConfigPinMap
{
public:
    enum PinMode {
        GPIO_IOMODE_NONE = -1,
        GPIO_OUTPUT,
        GPIO_INPUT,
        GPIO_INPUT_PULLUP,
        GPIO_INPUT_PULLDOWN
    };

    class PinDef {
    public:
        PinDef(const char* pinName, PinMode pinMode, int* pPinNum = nullptr, bool initialLevel = false)
        {
            _name = pinName;
            _pinMode = pinMode;
            _pPinNumber = pPinNum;
            _initialLevel = initialLevel;
            _pinNumber = 0;
        }
        String _name;
        PinMode _pinMode;
        bool _initialLevel;
        int _pinNumber;
        int* _pPinNumber;
    };

    // Min mappings for different processors
    static const char *_pinMapOtherStr[];
    static int _pinMapOtherPin[];
    static int _pinMapOtherLen;
    static int _pinMapD[];
    static int _pinMapDLen;
    static int _pinMapA[];
    static int _pinMapALen;

    // Get name of pin from string
    static int getPinFromName(const char *pinName);

    // Conversion from strings like:
    // GPIO_PULLUP, GPIO_PULLDOWN to GPIO_INPUT_PULLUP, etc
    static int getInputType(const char *inputTypeStr);

    // Configure multiple IOs
    static void configMultiple(ConfigBase& config, PinDef* pPinDefs, int numPinDefs, bool deinit=false);

    // Pin mode mapping
    static int mapPinModeToArduino(PinMode pinMode);
};
