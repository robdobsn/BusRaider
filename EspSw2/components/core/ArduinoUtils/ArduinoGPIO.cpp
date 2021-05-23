// ArduinoGPIO
// Rob Dobson 2012-2021

#include "ArduinoGPIO.h"
#include "stdint.h"
#include "ArduinoTime.h"
#include "esp_attr.h"
#include <driver/gpio.h>
#include "driver/adc.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Low-level GPIO functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" void IRAM_ATTR __pinMode(int pin, uint8_t mode)
{
    if (pin >= 0)
    {
        gpio_pad_select_gpio((gpio_num_t)pin);
        switch(mode)
        {
            case INPUT:
            case INPUT_PULLUP:
            case INPUT_PULLDOWN:
                gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
                if (mode == INPUT_PULLUP)
                    gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
                else if (mode == INPUT_PULLUP)
                    gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
                else
                    gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
                break;
            case OUTPUT:
                gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
                break;
        }
    }
}

extern "C" void IRAM_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
    gpio_set_level((gpio_num_t)pin, val);
}

extern "C" int IRAM_ATTR __digitalRead(uint8_t pin)
{
    return gpio_get_level((gpio_num_t)pin);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino-like functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" uint16_t IRAM_ATTR __analogRead(uint8_t pin)
{
    // Convert pin to adc channel
    // Only handles channel 1 (channel 2 generally available when WiFi used)
    adc1_channel_t analogChan = ADC1_CHANNEL_0;
    switch(pin)
    {
        case 36: analogChan = ADC1_CHANNEL_0; break;
        case 37: analogChan = ADC1_CHANNEL_1; break;
        case 38: analogChan = ADC1_CHANNEL_2; break;
        case 39: analogChan = ADC1_CHANNEL_3; break;
        case 32: analogChan = ADC1_CHANNEL_4; break;
        case 33: analogChan = ADC1_CHANNEL_5; break;
        case 34: analogChan = ADC1_CHANNEL_6; break;
        case 35: analogChan = ADC1_CHANNEL_7; break;
        default: 
            // Invalid channel - return 0 reading
            return 0;
    }

    // Configure width
    adc1_config_width(ADC_WIDTH_BIT_12);

    // Set attenuation (to allow voltages 0 .. 2.5V approx)
    adc1_config_channel_atten(analogChan, ADC_ATTEN_DB_11);

    // Get adc reading
    return adc1_get_raw(analogChan);
}

extern void pinMode(int pin, uint8_t mode) __attribute__ ((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__ ((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__ ((weak, alias("__digitalRead")));
extern uint16_t analogRead(uint8_t pin) __attribute__ ((weak, alias("__analogRead")));
