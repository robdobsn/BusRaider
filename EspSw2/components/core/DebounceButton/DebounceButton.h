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

typedef std::function<void(bool val, uint32_t msSinceLastChange, uint16_t repeatCount)> DebounceButtonCallback;

class DebounceButton
{
public:
    DebounceButton();
    virtual ~DebounceButton();

    // Setup
    static const uint32_t DEFAULT_PIN_DEBOUNCE_MS = 100;
    static const uint16_t DEFAULT_ACTIVE_REPEAT_MS = 500;

    // Setup
    void setup(int pin, bool pullup, bool activeLevel, 
                DebounceButtonCallback cb, 
                uint32_t debounceMs = DEFAULT_PIN_DEBOUNCE_MS,
                uint16_t activeRepeatTimeMs = DEFAULT_ACTIVE_REPEAT_MS);

    // Service - must be called frequently to check button state
    void service();

private:
    // Settings
    int16_t _buttonPin;
    bool _buttonActiveLevel;
    uint32_t _debounceMs;
    uint16_t _activeRepeatTimeMs;

    // State
    uint64_t _lastCheckMs;
    bool _firstPass;
    bool _lastStableVal;
    uint32_t _timeInPresentStateMs;
    uint64_t _lastRepeatTimeMs;
    uint16_t _repeatCount;
    static const uint32_t PIN_CHECK_MS = 10;

    // Callback
    DebounceButtonCallback _callback;
};
