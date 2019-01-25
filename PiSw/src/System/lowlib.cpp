
// Bus Raider
// Low-level library
// Rob Dobson 2019

#include "lowlib.h"
#include "nmalloc.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t micros()
{
    static const uint32_t volatile* pTimerLower32Bits = (uint32_t*)0x20003004;
    return *pTimerLower32Bits;
}

void microsDelay(uint32_t us)
{
    uint32_t timeNow = micros();
    while (!isTimeout(micros(), timeNow, us)) {
        // Do nothing
    }
}

int isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime) {
        return curTime > lastTime + maxDuration;
    }
    return ((ULONG_MAX - lastTime) + curTime) > maxDuration;
}

// Heap space
extern unsigned int pheap_space;
extern unsigned int heap_sz;

void heapInit()
{
    // Heap init
    nmalloc_set_memory_area((unsigned char*)(pheap_space), heap_sz);
}

#ifdef __cplusplus
}
#endif
