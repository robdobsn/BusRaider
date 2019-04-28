// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Unpaged size of target memory
static const uint32_t STD_TARGET_MEMORY_LEN = 0x10000;

// Return codes
enum BR_RETURN_TYPE {
    BR_OK = 0,
    BR_ERR = 1,
    BR_NO_BUS_ACK = 2,
    BR_ALREADY_DONE = 3,
    BR_NOT_HANDLED = 4
};

// Bus actions 
enum BR_BUS_ACTION {
    BR_BUS_ACTION_NONE, 
    BR_BUS_ACTION_RESET,
    BR_BUS_ACTION_NMI,
    BR_BUS_ACTION_IRQ,
    BR_BUS_ACTION_BUSRQ,
    BR_BUS_ACTION_PAGE_OUT_FOR_INJECT,
    BR_BUS_ACTION_PAGE_IN_FOR_INJECT
};

// Return codes from wait-state ISR

// Indicate that memory or IO requested is not supported by the machine
#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x80000000

// The returned value is an instruction to be injected into the target
// machine program flow - this is the mechanism used to set/get registers
#define BR_MEM_ACCESS_INSTR_INJECT 0x40000000

// The target processor should be held at this point - for single-stepping
#define BR_MEM_ACCESS_HOLD 0x20000000

// Control bus bits used to pass to machines, etc
#define BR_CTRL_BUS_RD 0
#define BR_CTRL_BUS_WR 1
#define BR_CTRL_BUS_MREQ 2
#define BR_CTRL_BUS_IORQ 3
#define BR_CTRL_BUS_M1 4
#define BR_CTRL_BUS_WAIT 5
#define BR_CTRL_BUS_RESET 6
#define BR_CTRL_BUS_IRQ 7
#define BR_CTRL_BUS_NMI 8
#define BR_CTRL_BUS_BUSRQ 9

// Control bit masks
#define BR_CTRL_BUS_RD_MASK (1 << BR_CTRL_BUS_RD)
#define BR_CTRL_BUS_WR_MASK (1 << BR_CTRL_BUS_WR)
#define BR_CTRL_BUS_MREQ_MASK (1 << BR_CTRL_BUS_MREQ)
#define BR_CTRL_BUS_IORQ_MASK (1 << BR_CTRL_BUS_IORQ)
#define BR_CTRL_BUS_M1_MASK (1 << BR_CTRL_BUS_M1)
#define BR_CTRL_BUS_WAIT_MASK (1 << BR_CTRL_BUS_WAIT)
#define BR_CTRL_BUS_RESET_MASK (1 << BR_CTRL_BUS_RESET)
#define BR_CTRL_BUS_IRQ_MASK (1 << BR_CTRL_BUS_IRQ)
#define BR_CTRL_BUS_NMI_MASK (1 << BR_CTRL_BUS_NMI)
#define BR_CTRL_BUS_BUSRQ_MASK (1 << BR_CTRL_BUS_BUSRQ)

#define MAX_REGISTER_SET_CODE_LEN 100
