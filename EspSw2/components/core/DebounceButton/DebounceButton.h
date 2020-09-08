/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Callback on button press via a hardware pin
//
// Rob Dobson 2018-2020
// Originally from RBotFirmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>

typedef std::function<void(int val, uint32_t time)> DebounceButtonCallback;

class DebounceButton
{
public:
    DebounceButton();
    // Setup
    static const uint32_t DEFAULT_PIN_DEBOUNCE_MS = 100;

    // Setup
    void setup(int pin, bool pullup, int activeLevel, 
                DebounceButtonCallback cb, 
                uint32_t debounceMs = DEFAULT_PIN_DEBOUNCE_MS,
                bool activateOnRelease = false);

    // Service - must be called frequently to check button state
    void service();

private:
    // Settings
    int _buttonPin;
    int _buttonActiveLevel;
    uint32_t _debounceMs;
    bool _activateOnRelease;

    // State
    uint32_t _lastCheckMs;
    bool _firstPass;
    bool _lastStableVal;
    uint32_t _changeCountMs;
    static const uint32_t PIN_CHECK_MS = 10;

    // Callback
    DebounceButtonCallback _callback;
};
