// Bus Raider
// Startup file for Raspberry Pi Zero
// Rob Dobson 2019

.section ".text.startup"

.global _startup
_startup:
    ldr pc, _reset_h
    ldr pc, _undefined_instruction_h
    ldr pc, _software_interrupt_h
    ldr pc, _prefetch_abort_h
    ldr pc, _data_abort_h
    ldr pc, _unused_handler_h
    ldr pc, _interrupt_h
    ldr pc, _fast_interrupt_h

_reset_h:                        .word   _reset_
    _undefined_instruction_h:    .word   /*undefined_instruction_*/ hang 
    _software_interrupt_h:       .word   /*software_interrupt_*/    hang
    _prefetch_abort_h:           .word   /*prefetch_abort_*/        hang
    _data_abort_h:               .word   /*data_abort_*/            hang
    _unused_handler_h:           .word   hang
    _interrupt_h:                .word   irq_handler_ 
    _fast_interrupt_h:           .word   fiq_handler_

// Linker script file used to set these definitions
.global bss_start
bss_start: .word __bss_start__

.global bss_end
bss_end: .word __bss_end__

.global pheap_space
pheap_space: .word _heap_start

.global heap_sz
heap_sz: .word heap_size

.global __otaUpdateBuffer
__otaUpdateBuffer: .word _otaUpdateBufferStart

// Program entry point
_reset_:

    // Processor is in supervisor mode at startup
    mov     r0, #0x8000
    mov     r1, #0x0000
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}
    mov sp, #0x8000

    // (PSR_IRQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    // Set interrupt mode stack pointer
    mov r0,#0xD2
    msr cpsr_c,r0
    mov sp,#0x8000

    // (PSR_FIQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    // Set FIQ mode stack pointer
    mov r0,#0xD1
    msr cpsr_c,r0
    mov sp,#0x4000

    // Back in supervisor mode set the stack pointer
    // (PSR_SVC_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    mov r0,#0xD3
    msr cpsr_c,r0
    mov sp,#0x8000000

// https://www.raspberrypi.org/forums/viewtopic.php?t=16851
// To enable L1 Cache on the ARMv6 CPU and enable branch prediction we set bits 11 and 12 in the Control Registor of CP15.
// Bit 12 enables the L1 instruction Cache, and is available in the StrongARM and all later ARM CPUs.
// Bit 11 enables branch prediction, and is only available in ARMv6 and all later CPUs.
// If you wish you can also enable Bit 2 to enable the Data Cache, though be careful with this one.
    MRC P15,0,R0,C1,C0
    MOV R1,#3
    ORR R0,R0,R1,LSL#11      // Set bits 11 and 12.
    MCR P15,0,R0,C1,C0       // Update the CP15 Control registor (C1) from R0.

    // Fill BSS with zeros
    ldr   r3, bss_start 
    ldr   r2, bss_end
    mov   r0, #0
1:
    cmp   r2, r3
    beq   2f
    str   r0, [r3]
    add   r3, r3, #1
    b     1b

2:
    // Jump to the entry point of the c/cpp code
    bl  entry_point

// FIQ (Fast Interrupt) handler
.global fiq_handler_
fiq_handler_:

    // FIQ swaps out R8..R14
    push {r0,r1,r2,r3,r4,r5,r6,r7,lr}
    bl c_firq_handler
    pop  {r0,r1,r2,r3,r4,r5,r6,r7,lr}
    subs pc,lr,#4

// In exceptions such as bad instruction
.global hang
hang: 
    b hang

