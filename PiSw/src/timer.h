// Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void _TimerHandler(unsigned hTimer, void* pParam, void* pContext);

extern void timers_init();
extern unsigned timer_attach_handler(unsigned hz, _TimerHandler* handler, void* pParam, void* pContext);
extern void timer_poll();

#ifdef __cplusplus
}
#endif
