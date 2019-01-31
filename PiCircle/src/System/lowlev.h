// Bus Raider
// Low-level utils
// Rob Dobson 2019

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Goto and store byte
extern void lowlevGoto(uint32_t gotoAddr);
extern void lowlevStoreAbs8(uint32_t addr, uint8_t byte);

// Interrupt enables and disables
extern void lowlevEnableIRQ();
extern void lowlevDisableIRQ();
extern void lowlevEnableFIQ();
extern void lowlevDisableFIQ();

// Delay at the machine cycle level
extern void lowlevCycleDelay(unsigned int cycles);

// Relocatable block copy (like memcpy but can be moved anywhere in memory)
extern void lowlevBlockCopyExecRelocatable(void* pDest, void* pSrc, int len, void* execAddr, void* stackPtr);
typedef void lowlevBlockCopyExecRelocatableFnT(uint8_t* pDest, uint8_t* pSrc, int len, void* execAddr, void* stackPtr);
extern unsigned int lowlevBlockCopyExecRelocatableLen;

#ifdef __cplusplus
}
#endif