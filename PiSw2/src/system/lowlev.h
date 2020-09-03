// Bus Raider
// Low-level utils
// Rob Dobson 2019-2020

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <circle/bcm2835.h>

// // Goto and store byte
// extern void lowlev_goto(uint32_t gotoAddr);
// extern void lowlev_store_abs8(uint32_t addr, uint8_t byte);

// // Interrupt enables and disables
// extern void lowlev_enable_irq();
// extern void lowlev_disable_irq();
// extern void lowlev_enable_fiq();
// extern void lowlev_disable_fiq();

// Delay at the machine cycle level
extern void lowlev_cycleDelay(unsigned int cycles);

// // Relocatable block copy (like memcpy but can be moved anywhere in memory)
// extern void* lowlev_blockCopyExecRelocatable(void* pDest, void* pSrc, int len, void* execAddr);
// typedef void* lowlev_blockCopyExecRelocatableFnT(uint8_t* pDest, uint8_t* pSrc, int len, void* execAddr);
// extern unsigned int lowlev_blockCopyExecRelocatableLen;

// extern void blinkCE0();
// extern void blinkLEDForever();

// extern void disable_mmu_and_cache();

// #define InvalidateInstructionCache()	asm volatile ("mcr p15, 0, %0, c7, c5,  0" : : "r" (0) : "memory")
// #define FlushPrefetchBuffer()	asm volatile ("mcr p15, 0, %0, c7, c5,  4" : : "r" (0) : "memory")
// #define FlushBranchTargetCache()	asm volatile ("mcr p15, 0, %0, c7, c5,  6" : : "r" (0) : "memory")
                
// // Data memory barrier - no memory access after this can complete until all
// // prior memory accesses are complete
// #define lowlev_dmb() asm volatile("mcr p15, #0, %[zero], c7, c10, #5"  :  : [zero] "r"(0))

// // Data synchronisation barrier - no instructions after this can run until all
// // prior instructions are complete
// #define lowlev_dsb() asm volatile("mcr p15, #0, %[zero], c7, c10, #4"  :  : [zero] "r"(0))

// // Clean entire cache and flush pending writes to memory
// #define lowlev_flushcache() asm volatile("mcr p15, #0, %[zero], c7, c14, #0"  : : [zero] "r"(0))

// #define lowlev_mem_p2v(X) (X)
// #define lowlev_mem_v2p(X) (X)
// #define lowlev_mem_2uncached(X) ((((unsigned int)X) & 0x0FFFFFFF) | 0x40000000)
// #define lowlev_mem_2cached(X) ((((unsigned int)X) & 0x0FFFFFFF))

// Fast memcpy
// TODO 2020
#define memcopyfast memcpy
// void *memcopyfast(void *pDest, const void *pSrc, uint32_t nLength);

#ifdef __cplusplus
}
#endif
