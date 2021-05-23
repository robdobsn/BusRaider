// Config Pin Map (maps as string to a pin on a processor)
// Rob Dobson 2016-2018

#pragma once

class ConfigBase;

static const int GPIO_IOMODE_NONE = -1;

class ConfigPinMap
{
public:
    enum {
        GPIO_OUTPUT,
        GPIO_INPUT,
        GPIO_INPUT_PULLUP,
        GPIO_INPUT_PULLDOWN
    };

    static const char *_pinMapOtherStr[];
    static int _pinMapOtherPin[];
    static int _pinMapOtherLen;
    static int _pinMapD[];
    static int _pinMapDLen;
    static int _pinMapA[];
    static int _pinMapALen;

    static int getPinFromName(const char *pinName);

    // Conversion from strings like:
    // GPIO_PULLUP, GPIO_PULLDOWN to GPIO_INPUT_PULLUP, etc
    static int getInputType(const char *inputTypeStr);

    // Configure multople IOs
    static void configMultiple(ConfigBase& config, const char* pPinNames[], int pinModeArray[], 
            int pinInitial[], int* pinValues[], int numPins);
};
