// Bus Raider
// Rob Dobson 2019

#pragma once

#include "BCM2835.h"
#include "lowlib.h"
#include "CInterrupts.h"

typedef void TimerCallbackFnType(void* pParam);

class Timers
{
public:

    static void timerISR([[maybe_unused]] void* pParam)
    {
        // Clear and set next
    	WR32(ARM_SYSTIMER_CS, 1<<3);
        uint32_t curLowCount = RD32(ARM_SYSTIMER_CLO);
        WR32(ARM_SYSTIMER_C3, curLowCount + _timerNextInc);

        // Using system timer 3
        if (_timerEnabled && _pTimerCallback)
            _pTimerCallback(pParam);
    }

    static void set(int durationUs, TimerCallbackFnType* pTimerFn, void* pParam, bool enable=true)
    {
        // Using system timer 3        
        _pTimerCallback = pTimerFn;
        _pParam = pParam;
        _timerEnabled = enable;
        _timerNextInc = durationUs * ARM_SYSTIMER_RATE / 1000000;

        // Set the interrupt controller
        CInterrupts::connectIRQ(ARM_IRQ_TIMER3, Timers::timerISR, 0);

        // Set compare register initially
        uint32_t curLowCount = RD32(ARM_SYSTIMER_CLO);
        WR32(ARM_SYSTIMER_C3, curLowCount + _timerNextInc);
    }
    static void start()
    {
        _timerEnabled = true;
    }
    static void stop()
    {
        _timerEnabled = false;
    }

private:
    static TimerCallbackFnType* _pTimerCallback;
    static bool _timerEnabled;
    static void* _pParam;
    static uint32_t _timerNextInc;
};
