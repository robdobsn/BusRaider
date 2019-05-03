
// Bus Raider
// Low-level library
// Rob Dobson 2019

#include "lowlib.h"
#include "CInterrupts.h"
#include <limits.h>
#include "memorymap.h"
#include "nmalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t micros()
{
    static const uint32_t volatile* pTimerLower32Bits = (uint32_t*)ARM_SYSTIMER_CLO;
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

// Startup code
extern "C" void entry_point()
{
    // Start and end points of the constructor list,
    // defined by the linker script.
    extern void (*__init_array_start)();
    extern void (*__init_array_end)();

    // Call each function in the list.
    // We have to take the address of the symbols, as __init_array_start *is*
    // the first function pointer, not the address of it.
    for (void (**p)() = &__init_array_start; p < &__init_array_end; ++p) {
        (*p)();
    }

    // Heap init
    nmalloc_set_memory_area((unsigned char*)(MEM_HEAP_START), MEM_HEAP_SIZE);

    // Interrupts
    CInterrupts::setup();

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
