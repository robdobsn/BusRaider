#pragma once

#include "globaldefs.h"

extern uint32_t micros();
extern void delayMicroseconds(uint32_t us);

typedef void _TimerHandler(unsigned hTimer, void* pParam, void* pContext);

extern void timers_init();
extern unsigned timer_attach_handler(unsigned hz, _TimerHandler* handler, void* pParam, void* pContext);
extern void timer_poll();
