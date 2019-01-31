// Bus Raider
// Low-level utils
// Rob Dobson 2019

#include "lowlev.h"
#include <circle/bcm2835.h>

#ifdef __cplusplus
extern "C" {
#endif

void lowlevDisableIRQ()
{
    asm volatile
    (
        "cpsid i"
    );
}

void lowlevEnableIRQ()
{
    asm volatile
    (
        "cpsie i"
    );
}

void lowlevDisableFIQ()
{
    asm volatile
    (
        "cpsid f"
    );
}

void lowlevEnableFIQ()
{
    asm volatile
    (
        "cpsie f"
    );
}

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
// #define ARM_PM_PASSWD_OR_ONE (ARM_PM_PASSWD | 1)
// #define ARM_PM_PASSWD_OR_RSTC_WRCFG_FULL_RESET (ARM_PM_PASSWD | 0x20)
void lowlevBlockCopyExecRelocatable(void* pDest, void* pSrc, int len, void* execAddr, void* stackPtr)
{
    // static const uint32_t ARM_PM_WDOG_VALUE = ARM_PM_WDOG;
    // static const uint32_t ARM_PM_PASSWD_OR_ONE = (ARM_PM_PASSWD | 1);
    asm volatile
    (
    // "cpsid a\n"
    // "cpsid i\n"
    // "cpsid f\n"

    "ldr r0, =(0x20200020)\n"
    "ldr r1, =(1 << 15)\n"
    "str r1, [r0]\n"
    "bx	lr\n"

//     "mov r0, %0\n"
//     "mov r1, %1\n"
//     "mov r2, %2\n"
//     "mov r3, %3\n"
//     "mov sp, %4\n"
//     "mov r4, r3\n"
// "blockCopyExecRelocatableLoop:\n"
//     "LDRB r3, [r1], #1\n"
//     "STRB r3, [r0], #1\n"
//     "SUBS r2, r2, #1\n"
//     "BGE blockCopyExecRelocatableLoop\n"

    // "b 0x8000\n"

// 	// write32 (ARM_PM_WDOG, ARM_PM_PASSWD | 1);	// set some timeout
//     "ldr r0, =(0x20000000 + 0x100000 + 0x24)\n"
//     "ldr r1, =((0x5A << 24) | 1)\n"
//     "str r1, [r0]\n"
//     // #define PM_RSTC_WRCFG_FULL_RESET	0x20
// 	// write32 (ARM_PM_RSTC, ARM_PM_PASSWD | PM_RSTC_WRCFG_FULL_RESET);
//     "ldr r0, =(0x20000000 + 0x100000 + 0x1C)\n"
//     "ldr r1, =((0x5A << 24) | 0X20)\n"
//     "str r1, [r0]\n"
    
// "blockWaitForResetLoop:\n"
//     "b blockWaitForResetLoop\n"

"blockCopyExecRelocatableEnd:\n"
".global lowlevBlockCopyExecRelocatableLen\n"
"lowlevBlockCopyExecRelocatableLen: .word blockCopyExecRelocatableEnd-lowlevBlockCopyExecRelocatable\n"
    // : : "r" (pDest), "r" (pSrc), "r" (len), "r" (execAddr), "r" (stackPtr)
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
