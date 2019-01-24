// Bus Raider
// Low-level utils
// Rob Dobson 2019

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bare_metal_pi_zero.h"

extern void lowlev_goto(uint32_t gotoAddr);
extern void lowlev_store_abs8(uint32_t addr, uint8_t byte);

extern void lowlev_enable_irq();
extern void lowlev_disable_irq();
extern void lowlev_enable_fiq();
extern void lowlev_disable_fiq();

extern void lowlev_cycleDelay(unsigned int cycles);
// extern void lowlev_membarrier();

/**
 * String related
 */
extern unsigned int lowlev_strlen(const char* str);
extern int lowlev_strcmp(const char* s1, const char* s2);

/**
 * Relocatable block copy (like memcpy but can be moved anywhere in memory)
 */
extern void* lowlev_blockCopyExecRelocatable(void* pDest, void* pSrc, int len, void* execAddr);
typedef void* lowlev_blockCopyExecRelocatableFnT(uint8_t* pDest, uint8_t* pSrc, int len, void* execAddr);
extern unsigned int lowlev_blockCopyExecRelocatableLen;

/*


inline void memcpy( unsigned char* dst, unsigned char* src, unsigned int len )
{
    unsigned char* srcEnd = src + len;
    while( src<srcEnd )
    {
        *dst++ =  *src++;
    }
}
*/

/*
 *   Data memory barrier
 *   No memory access after the DMB can run until all memory accesses before it
 *    have completed
 *    
 */
#define lowlev_dmb() asm volatile("mcr p15, #0, %[zero], c7, c10, #5" \
                           :                                   \
                           : [zero] "r"(0))

/*
 *  Data synchronisation barrier
 *  No instruction after the DSB can run until all instructions before it have
 *  completed
 */
#define lowlev_dsb() asm volatile("mcr p15, #0, %[zero], c7, c10, #4" \
                           :                                   \
                           : [zero] "r"(0))

/*
 * Clean and invalidate entire cache
 * Flush pending writes to main memory
 * Remove all data in data cache
 */
#define lowlev_flushcache() asm volatile("mcr p15, #0, %[zero], c7, c14, #0" \
                                  :                                   \
                                  : [zero] "r"(0))

#define lowlev_mem_p2v(X) (X)
#define lowlev_mem_v2p(X) (X)
//#define mem_2uncached(X) (X)
//#define mem_2cached(X)   (X)

//#define mem_p2v(X) ((((unsigned int)X)&0x0FFFFFFF)|0x40000000)
//#define mem_v2p(X) ((((unsigned int)X)&0x0FFFFFFF))
#define lowlev_mem_2uncached(X) ((((unsigned int)X) & 0x0FFFFFFF) | 0x40000000)
#define lowlev_mem_2cached(X) ((((unsigned int)X) & 0x0FFFFFFF))

#ifdef __cplusplus
}
#endif