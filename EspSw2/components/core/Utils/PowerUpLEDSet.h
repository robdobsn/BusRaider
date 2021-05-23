// PowerUpLEDSet
// Rob Dobson 2020

#pragma once

#include <WS2812.h>
#include <Logger.h>
#include <ConfigBase.h>
#include <Utils.h>
#include <ArduinoGPIO.h>
#include <ConfigPinMap.h>

// #define DEBUG_POWER_LED_SETUP

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void powerUpLEDSet(const char *pModuleName, ConfigBase &mainConfig)
{
    // Get configuration
    ConfigBase config = mainConfig.getString(pModuleName, "{}");

    // Check enabled
    bool isEnabled = config.getLong("enable", 0) != 0;
#ifdef DEBUG_POWER_LED_SETUP
    LOG_I("PowerUpLEDSet", "setup, enable %d", isEnabled);
#endif
    if (!isEnabled)
        return;

    // Get LED pin
    String ledPinStr = config.getString("ledPin", "");
    int ledPin = -1;
    if (ledPinStr.length() != 0)
        ledPin = ConfigPinMap::getPinFromName(ledPinStr.c_str());
#ifdef DEBUG_POWER_LED_SETUP
    LOG_I("PowerUpLEDSet", "setup, ledPin %d", ledPin);
#endif
    if (ledPin < 0)
        return;

    // Get LED settings and default ledVal
    int ledVal = config.getLong("ledVal", 0);
    int isPix = config.getLong("isPix", 0);
    int pixIdx = config.getLong("pixIdx", 0);

    // Check for USB sensing pin
    String usbSensePinStr = config.getString("usbSnsPin", "");
    int usbSenseThreshold = config.getLong("usbSnsThresh", 0);
    int usbSenseLedVal = config.getLong("usbSnsVal", 0);
    int usbSensePin = -1;
    if (usbSensePinStr.length() != 0)
        usbSensePin = ConfigPinMap::getPinFromName(usbSensePinStr.c_str());
#ifdef DEBUG_POWER_LED_SETUP
    LOG_I("PowerUpLEDSet", "setup, usbSensePin %d usbSenseThreshold %d usbSenseLedVal %06x", 
                usbSensePin, usbSenseThreshold, usbSenseLedVal);
#endif
    if (usbSensePin >= 0)
    {
        uint16_t adcVal = analogRead(usbSensePin);
        if (adcVal > usbSenseThreshold)
            ledVal = usbSenseLedVal;
    }

#ifdef DEBUG_POWER_LED_SETUP
    LOG_I("PowerUpLEDSet", "setup, isPix %d ledVal %06x", isPix, ledVal);
#endif

    // Check if pixel strip
    if (isPix)
    {
        // Setup pixel strip
        WS2812 pixStrip;
        esp_err_t espError = pixStrip.setup(RMT_CHANNEL_0, ledPin, pixIdx+1);
        if (espError != ESP_OK)
        {
            LOG_W("PowerUpLEDSet", "setup, Failed to setup WS2812 driver, espError %d", espError);
            return;
        }
        else
        {
#ifdef DEBUG_POWER_LED_SETUP
            LOG_I("PowerUpLEDSet", "setup ok");
#endif
        }
        pixStrip.setPixelColor(pixIdx, ledVal);
        pixStrip.show();
    }
    else
    {
        gpio_set_direction((gpio_num_t)ledPin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)ledPin, ledVal);
    }
}
