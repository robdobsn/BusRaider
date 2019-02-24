// Bus Raider
// Low-level library
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include "lowlev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WR32(addr, val) (*(volatile unsigned *)(addr)) = (val)
#define RD32(addr) (*(volatile unsigned *)(addr))

extern uint32_t micros();
extern void microsDelay(uint32_t us);
extern int isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration);
extern void heapInit();

#define PeripheralEntry()	lowlev_dsb()
#define PeripheralExit()	lowlev_dmb()

#define PACKED		__attribute__ ((packed))
#define	ALIGN(n)	__attribute__ ((aligned (n)))
#define NORETURN	__attribute__ ((noreturn))
#define NOOPT		__attribute__ ((optimize (0)))
#define MAXOPT		__attribute__ ((optimize (3)))
#define WEAK		__attribute__ ((weak))

#ifdef __cplusplus
}
#endif