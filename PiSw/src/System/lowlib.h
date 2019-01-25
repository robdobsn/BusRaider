// Bus Raider
// Low-level library
// Rob Dobson 2019

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WR32(addr, val) (*(volatile unsigned *)(addr)) = (val)
#define RD32(addr) (*(volatile unsigned *)(addr))

extern uint32_t micros();
extern void microsDelay(uint32_t us);
extern int isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
extern void heapInit();

#ifdef __cplusplus
}
#endif