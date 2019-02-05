
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

// Startup code
extern "C" void entry_point()
{
    // call construtors of static objects
	extern void (*__init_start) (void);
	extern void (*__init_end) (void);
	for (void (**pFunc) (void) = &__init_start; pFunc < &__init_end; pFunc++)
	{
		(**pFunc) ();
	}

    // Main function
    extern int main (void);
    main();
}

// CXX 

// extern void __aeabi_unwind_cpp_pr0(void);
// extern void __cxa_end_cleanup(void);

// Error handler for pure virtual functions
// extern void __cxa_pure_virtual();
void __cxa_pure_virtual()
{
}

// // Stubbed out exception routine to avoid unwind errors in ARM code
// // https://stackoverflow.com/questions/14028076/memory-utilization-for-unwind-support-on-arm-architecture
// void __aeabi_unwind_cpp_pr0(void) 
// {

// }

// // Stubbed out - probably should restore registers but might only be necessary on unwinding after exceptions
// // which are not used
// void __cxa_end_cleanup(void)
// {

// }

extern "C" int __aeabi_atexit( 
    void *object, 
    void (*destructor)(void *), 
    void *dso_handle) 
{ 
    static_cast<void>(object); 
    static_cast<void>(destructor); 
    static_cast<void>(dso_handle); 
    return 0; 
} 

void* __dso_handle = nullptr;

#ifdef __cplusplus
}
#endif
