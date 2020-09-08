/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Callback on button press via a hardware pin
//
// Rob Dobson 2018-2020
// Originally from RBotFirmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include "DebounceButton.h"
#include <Utils.h>
#include "Logger.h"
#include <driver/gpio.h>

// Constructor
DebounceButton::DebounceButton()
{
    _buttonPin = -1;
    _buttonActiveLevel = 0;
    _lastCheckMs = 0;
    _firstPass = true;
    _lastStableVal = false;
    _changeCountMs = 0;
    _callback = nullptr;
}

// Setup
void DebounceButton::setup(int pin, bool pullup, int activeLevel, 
        DebounceButtonCallback cb, uint32_t debounceMs, bool activateOnRelease)
{
    // Settings
    _buttonPin = pin;
    _buttonActiveLevel = activeLevel;
    _debounceMs = debounceMs;
    _activateOnRelease = activateOnRelease;

    // State
    _lastCheckMs = millis();
    _firstPass = true;
    _lastStableVal = 0;
    _changeCountMs = 0;
    _callback = cb;

    // Setup the input pin
    if (_buttonPin >= 0)
    {
        gpio_pad_select_gpio((gpio_num_t)_buttonPin);
        gpio_set_direction((gpio_num_t)_buttonPin, GPIO_MODE_INPUT);
        if (pullup)
            gpio_set_pull_mode((gpio_num_t)_buttonPin, GPIO_PULLUP_ONLY);
    }
}

void DebounceButton::service()
{
    // Pin valid check
    if (_buttonPin < 0)
        return;

    // Check time for check
    if (Utils::isTimeout(millis(), _lastCheckMs, PIN_CHECK_MS))
    {
        // Count time elapsed in changed state
        _changeCountMs += (millis() - _lastCheckMs);

        // Last check update
        _lastCheckMs = millis();

        // Check first time we've monitored
        if (_firstPass)
        {
            _lastStableVal = gpio_get_level((gpio_num_t)_buttonPin) == _buttonActiveLevel;
            _firstPass = false;
            return;
        }

        // Check for change of state
        bool curVal = gpio_get_level((gpio_num_t)_buttonPin) == _buttonActiveLevel;

        // Check if changed
        if (curVal != _lastStableVal)
        {
            // See if at threshold for detection
            if (_changeCountMs > _debounceMs )
            {
                // Set active/inactive
                _lastStableVal = curVal;

                // Call callback
                if (_callback)
                    _callback(curVal, _changeCountMs);
            }
            if (_activateOnRelease)
                _changeCountMs = 0;
        }
        else
        {
            if (!_activateOnRelease)
                _changeCountMs = 0;
        }
    }
}
