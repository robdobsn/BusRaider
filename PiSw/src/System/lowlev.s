// Bus Raider
// Low-level utils
// Rob Dobson 2019

.global lowlev_enable_irq
lowlev_enable_irq:
    mrs r0,cpsr
    bic r0,r0,#0x80
    msr cpsr_c,r0
    bx lr
    ;@cpsie i
    ;@mov pc, lr

.global lowlev_disable_irq
lowlev_disable_irq:
        cpsid i
        mov pc, lr

.global lowlev_enable_fiq
lowlev_enable_fiq:
    mrs r0,cpsr
    bic r0,r0,#0x40
    msr cpsr_c,r0
    bx lr

.global lowlev_disable_fiq
lowlev_disable_fiq:
    cpsid f
    mov     pc, lr

;@ Wait for at least r0 cycles
.global lowlev_cycleDelay
lowlev_cycleDelay:
        push {r1}
        mov r0, r0, ASR #1
bloop:  ;@ eor r1, r0, r1
        subs r0,r0,#1
        bne bloop
        
        ;@ quit
        pop {r1}
        bx lr   ;@ Return 

;@ performs a memory barrier
;@ http://infocenter.arm.com/help/topic/com.arm.doc.ddi0360f/I1014942.html
;@
; .global membarrier
; membarrier:
;     push {r3}
;     mov r3, #0                      ;@ The read register Should Be Zero before the call
;     mcr p15, 0, r3, C7, C6, 0       ;@ Invalidate Entire Data Cache
;     mcr p15, 0, r3, c7, c10, 0      ;@ Clean Entire Data Cache
;     mcr p15, 0, r3, c7, c14, 0      ;@ Clean and Invalidate Entire Data Cache
;     mcr p15, 0, r3, c7, c10, 4      ;@ Data Synchronization Barrier
;     mcr p15, 0, r3, c7, c10, 5      ;@ Data Memory Barrier
;     bx lr

.global disable_mmu_and_cache
disable_mmu_and_cache:

; mrc p15, 0, r0, c1, c0, 0
; bic r0,r0,#0x1000 ;@ instruction
; bic r0,r0,#0x0004 ;@ data
; mcr p15, 0, r0, c1, c0, 0

    push {r0,r1,r2,r3,r10}
    mov r3, #0                      ;@ The read register Should Be Zero before the call
    mcr p15, 0, r3, C7, C6, 0       ;@ Invalidate Entire Data Cache
    mcr p15, 0, r3, c7, c10, 0      ;@ Clean Entire Data Cache
    mcr p15, 0, r3, c7, c14, 0      ;@ Clean and Invalidate Entire Data Cache
    mcr p15, 0, r3, c7, c10, 4      ;@ Data Synchronization Barrier
    mcr p15, 0, r3, c7, c10, 5      ;@ Data Memory Barrier
    
    mrc p15, 0, r3, c1, c0, 0
    mov r0, #5
    bic r3, r3, r0, LSL #0             ;@ Disable MMU, L1 data cache
    mov r0, #3
    bic r3, r3, r0, LSL #11            ;@ Disable Prefetch branch, L1 instruction
    mcr p15, 0, r3, c1, c0, 0
    pop {r0,r1,r2,r3,r10}
    bx lr

.equ ABASE,  0x20200000 @Base address
.equ AGPFSEL0, 0x00			@FSEL register offset 
.equ AGPFSEL4, 0x10
.equ AGPSET0,  0x1c			@GPSET0 register offset
.equ AGPSET1,  0x20
.equ AGPCLR0,  0x28			@GPCLR0 register offset
.equ ACOUNTER, 0x100
.equ SET_BIT15,  0x8000
.equ SET_BIT21,  0x200000
.equ SET_BIT24,  0x1000000 	@sets bit 24
.equ AOUTPIN,  0x100 	@sets bit 8
.equ AGPCLR1,  0x2c
.equ BCOUNTER, 0xf00000

// Debugging code to blink SPI0 CE0 pin
.global blinkCE0
blinkCE0:

    push {r0,r1,r2,r3,r10}
    ldr r0,=ABASE
    ldr r1,=SET_BIT24
    str r1,[r0,#AGPFSEL0]
    ldr r1,=AOUTPIN
    ldr r2,=ACOUNTER
    str r1,[r0,#AGPSET0]
	mov r10,#0
delay:
    add r10,r10,#1
    cmp r10,r2	
    bne delay
	str r1,[r0,#AGPCLR0]
	mov r10,#0
delay2:
    add r10,r10,#1
    cmp r10,r2	
    bne delay2
    pop {r0,r1,r2,r3,r10}
    bx lr   ;@ Return 

.global blinkLEDForever
blinkLEDForever:
    ldr r0,=ABASE
    ldr r2,=BCOUNTER
    ldr r1,=SET_BIT21
    str r1,[r0,#AGPFSEL4]
    ldr r1,=SET_BIT15
    str r1,[r0,#AGPSET1]

loopBlink:
    str r1,[r0,#AGPSET1]	

	mov r10,#0
delay1a:
    add r10,r10,#1
    cmp r10,r2	
    bne delay1a

	str r1,[r0,#AGPCLR1]

	mov r10,#0
delay2a:
    add r10,r10,#1
    cmp r10,r2	
    bne delay2a
    b loopBlink

// blockCopyExecRelocatable - copied to heap and used for firmware update
// params: dest, source, len, execAddr
.global lowlev_blockCopyExecRelocatable
lowlev_blockCopyExecRelocatable:

    push {r3}

blockCopyExecRelocatableLoop:
    LDRB r3, [r1], #1
    STRB r3, [r0], #1
    SUBS r2, r2, #1
    BGE blockCopyExecRelocatableLoop
    pop {r0}
    bx r0

blockCopyExecRelocatableEnd:
.global lowlev_blockCopyExecRelocatableLen
lowlev_blockCopyExecRelocatableLen: .word blockCopyExecRelocatableEnd-lowlev_blockCopyExecRelocatable

.global lowlev_goto
lowlev_goto:
    bx r0

.globl lowlev_store_abs8
lowlev_store_abs8:
    strb r1,[r0]
    bx lr
