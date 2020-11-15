// PowerUpLEDSet
// Rob Dobson 2020

#pragma once

#include <WS2812.h>
#include <Logger.h>
#include <ConfigBase.h>
#include <Utils.h>
#include <ConfigPinMap.h>

// #define DEBUG_POWER_LED_SETUP

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void powerUpLEDSet(ConfigBase config)
{
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

    // Get value
    int ledVal = config.getLong("ledVal", 0);
    int isPix = config.getLong("isPix", 0);
    int pixIdx = config.getLong("pixIdx", 0);

#ifdef DEBUG_POWER_LED_SETUP
    LOG_I("PowerUpLEDSet", "setup, isPix %d ledVal %d", isPix, ledVal);
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
        delay(100);
    }
    else
    {
        gpio_set_direction((gpio_num_t)ledPin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)ledPin, ledVal);
    }
}
