// Bus Raider
// Rob Dobson 2018

#include "ee_printf.h"
#include "rdutils.h"
#include "timer.h"

uint32_t micros()
{
    static const uint32_t volatile* pTimerLower32Bits = (uint32_t*)0x20003004;
    return *pTimerLower32Bits;
}

void delayMicroseconds(uint32_t us)
{
    uint32_t timeNow = micros();
    while (!timer_isTimeout(micros(), timeNow, us)) {
        // Do nothing
    }
}

typedef struct
{
    _TimerHandler* handler;
    void* pParam;
    void* pContext;
    unsigned int microsec_interval;
    unsigned int last_tick;

} TimerUnit;

#define N_TIMERS 20
static TimerUnit timers[N_TIMERS];

void timers_init()
{
    int i;
    for (i = 0; i < N_TIMERS; ++i) {
        timers[i].handler = 0;
    }
}

unsigned timer_attach_handler(unsigned hz,
    _TimerHandler* handler,
    void* pParam,
    void* pContext)
{
    unsigned hnd;
    for (hnd = 0; hnd < N_TIMERS; ++hnd) {
        if (timers[hnd].handler == 0) {
            timers[hnd].handler = handler;
            timers[hnd].pParam = pParam;
            timers[hnd].pContext = pContext;
            timers[hnd].microsec_interval = 1000000 / hz;
            timers[hnd].last_tick = micros();
            return hnd;
        }
    }

    return N_TIMERS + 1;
}

void timer_poll()
{
    unsigned hnd;
    unsigned int tnow = micros();

    for (hnd = 0; hnd < N_TIMERS; ++hnd) {
        if (timers[hnd].handler != 0 && (tnow - timers[hnd].last_tick) > timers[hnd].microsec_interval) {
            _TimerHandler* handler = timers[hnd].handler;
            timers[hnd].handler = 0;
            handler(hnd, timers[hnd].pParam, timers[hnd].pContext);
        }
    }
}
