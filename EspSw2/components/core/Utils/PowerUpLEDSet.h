// PowerUpLEDSet
// Rob Dobson 2020

#pragma once

#include <WS2812.h>
#include <Logger.h>
#include <ConfigBase.h>
#include <Utils.h>
#include <ConfigPinMap.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void powerUpLEDSet(ConfigBase config)
{
    // Check enabled
    bool isEnabled = config.getLong("enable", 0) != 0;
    LOG_W("PowerUpLEDSet", "setup, conf %s enable %d", config.c_str(), isEnabled);
    if (!isEnabled)
        return;

    // Get LED pin
    String ledPinStr = config.getString("ledPin", "");
    int ledPin = -1;
    if (ledPinStr.length() != 0)
        ledPin = ConfigPinMap::getPinFromName(ledPinStr.c_str());
    LOG_W("PowerUpLEDSet", "setup, ledPin %d", ledPin);
    if (ledPin < 0)
        return;

    // Get value
    int ledVal = config.getLong("ledVal", 0);
    int isPix = config.getLong("isPix", 0);
    int pixIdx = config.getLong("pixIdx", 0);

    LOG_W("PowerUpLEDSet", "setup, isPix %d ledVal %d", isPix, ledVal);

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
            LOG_W("PowerUpLEDSet", "setup ok");
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
