// Bus Raider
// Rob Dobson 2018

#include "rdutils.h"
#include "globaldefs.h"
#include "nmalloc.h"

int timer_isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime) {
        return (curTime > lastTime + maxDuration);
    }
    return (ULONG_MAX - (lastTime - curTime) > maxDuration);
}

// Heap space
extern unsigned int pheap_space;
extern unsigned int heap_sz;

void system_init()
{
    // Heap init
    nmalloc_set_memory_area((unsigned char*)(pheap_space), heap_sz);

}