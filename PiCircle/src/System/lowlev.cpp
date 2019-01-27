// Bus Raider
// Low-level utils
// Rob Dobson 2019

#include "lowlev.h"

#ifdef __cplusplus
extern "C" {
#endif

void lowlevCycleDelay(unsigned int cycles)
{
    asm volatile
    (
        "push {r1}\n"
        "mov r0, %0\n"
        "mov r0, r0, ASR #1\n"
"loopit:"  
        "subs r0,r0,#1\n"
        "bne loopit\n"
        "pop {r1}\n"
        : : "r" (cycles)
    );
}

// blockCopyExecRelocatable - copied to heap and used for firmware update
// params: dest, source, len, execAddr
void lowlevBlockCopyExecRelocatable(void* pDest, void* pSrc, int len, void* execAddr)
{
    asm volatile
    (
    "mov r0, %0\n"
    "mov r1, %1\n"
    "mov r2, %2\n"
    "mov r3, %3\n"
    "push {r3}\n"
"blockCopyExecRelocatableLoop:\n"
    "LDRB r3, [r1], #1\n"
    "STRB r3, [r0], #1\n"
    "SUBS r2, r2, #1\n"
    "BGE blockCopyExecRelocatableLoop\n"
    "pop {r0}\n"
    "bx r0\n"
"blockCopyExecRelocatableEnd:\n"
".global lowlevBlockCopyExecRelocatableLen\n"
"lowlevBlockCopyExecRelocatableLen: .word blockCopyExecRelocatableEnd-lowlevBlockCopyExecRelocatable\n"
    : : "r" (pDest), "r" (pSrc), "r" (len), "r" (execAddr)
    );
}

void lowlevGoto(uint32_t gotoAddr)
{
    asm volatile
    (
    "mov r0, %0\n"
    "bx r0\n"
    : : "r" (gotoAddr)
    );
}

void lowlevStoreAbs8(uint32_t addr, uint8_t byte)
{
    asm volatile
    (
    "mov r0, %0\n"
    "mov r1, %1\n"
    "strb r1,[r0]\n"
    "bx lr\n"
    : : "r" (addr), "r" (byte)
    );
}

#ifdef __cplusplus
}
#endif
