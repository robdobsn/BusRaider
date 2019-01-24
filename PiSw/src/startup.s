// Bus Raider
// Startup file for Raspberry Pi Zero
// Rob Dobson 2019

.global bootstrap
bootstrap:
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


;@ See linker script file
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

;@ Initial entry point
_reset_:
    mov     r0, #0x8000
    mov     r1, #0x0000
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}
    mov sp, #0x8000

    ;@ (PSR_IRQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    mov r0,#0xD2
    msr cpsr_c,r0
    mov sp,#0x8000

    ;@ (PSR_FIQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    mov r0,#0xD1
    msr cpsr_c,r0
    mov sp,#0x4000

    ;@ (PSR_SVC_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
    mov r0,#0xD3
    msr cpsr_c,r0
    mov sp,#0x8000000

    ;@ Fill BSS with zeros
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
    ;@ Jump to the entry point
    bl  entry_point

.global fiq_handler_
fiq_handler_:

    # ldr r8, =#0x20200000
    # mov r9, #0x100
    # mvn r10, #0
    # mov r11, #0x3000
    # str r9, [r8, #0x1C]
    # str r10, [r8, #0x40]
    # str r11, [r8, #0x28]
    # str r11, [r8, #0x1C]
    # str r9, [r8, #0x28]
    # subs pc,lr,#4




    ;@ FIQ swaps out R8..R14
    push {r0,r1,r2,r3,r4,r5,r6,r7,lr}
    bl c_firq_handler
    pop  {r0,r1,r2,r3,r4,r5,r6,r7,lr}
    subs pc,lr,#4

.global hang
hang: 
    b hang

