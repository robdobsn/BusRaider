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
.global membarrier
membarrier:
    push {r3}
    mov r3, #0                      ;@ The read register Should Be Zero before the call
    mcr p15, 0, r3, C7, C6, 0       ;@ Invalidate Entire Data Cache
    mcr p15, 0, r3, c7, c10, 0      ;@ Clean Entire Data Cache
    mcr p15, 0, r3, c7, c14, 0      ;@ Clean and Invalidate Entire Data Cache
    mcr p15, 0, r3, c7, c10, 4      ;@ Data Synchronization Barrier
    mcr p15, 0, r3, c7, c10, 5      ;@ Data Memory Barrier
    bx lr


;@ strlen
.global strlen
strlen:
    push {r1,r2}
    mov r1, r0 
    mov r0, #0
1:
    ldrb r2, [r1]
    cmp r2,#0
    beq 2f
    add r0, #1
    add r1, #1
    b 1b
2:
    pop {r1,r2}
    bx lr


;@ strcmp
;@  r0: string 1 address
;@  r1: string 2 address
;@  output (r0):
;@  0 if the two strings are equal
;@    a negative number if s1<s2
;@    a positive number otherwise
.global strcmp
strcmp:
    push {r2,r3}

1:
    ldrb r2, [r0]
    ldrb r3, [r1]  
    add r0,r0,#1
    add r1,r1,#1

    cmp r2, #0
    beq 1f
    cmp r3, #0
    beq 1f
    
    cmp r2, r3
    beq 1b
1:
    sub r0, r2, r3
    pop {r2,r3}
    bx lr

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

.global utils_goto
utils_goto:
    bx r0

.globl utils_store_abs8
utils_store_abs8:
    strb r1,[r0]
    bx lr
