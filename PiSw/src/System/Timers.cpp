// Bus Raider
// Rob Dobson 2019

#include "Timers.h"
#include "stddef.h"

TimerCallbackFnType* Timers::_pTimerCallback = NULL;
bool Timers::_timerEnabled = false;
void* Timers::_pParam = NULL;
uint32_t Timers::_timerNextInc = 1000;
