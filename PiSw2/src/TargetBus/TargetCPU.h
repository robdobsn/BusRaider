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

// Bus action reasons
enum BR_BUS_REQ_REASON {
    // Request for display memory access
    BR_BUS_REQ_REASON_DISPLAY,
    // Request to mirror memory during debugging
    BR_BUS_REQ_REASON_MIRROR,
    // Request to program the target memory
    BR_BUS_REQ_REASON_PROGRAMMING,
    // Hardware access
    BR_BUS_REQ_REASON_HW_ACTION,
    // General indicator - used when bus action is not bus mastering
    BR_BUS_REQ_REASON_GENERAL
};

// Return codes from wait-state ISR

// Indicate that memory or IO requested is not supported by the machine
#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x80000000

// The returned value is an instruction to be injected into the target
// machine program flow - this is the mechanism used to set/get registers
#define BR_MEM_ACCESS_INSTR_INJECT 0x40000000

// Control bus bits used to pass to machines, etc
#define BR_CTRL_BUS_RD_BITNUM 0
#define BR_CTRL_BUS_WR_BITNUM 1
#define BR_CTRL_BUS_MREQ_BITNUM 2
#define BR_CTRL_BUS_IORQ_BITNUM 3
#define BR_CTRL_BUS_M1_BITNUM 4
#define BR_CTRL_BUS_WAIT_BITNUM 5
#define BR_CTRL_BUS_RESET_BITNUM 6
#define BR_CTRL_BUS_IRQ_BITNUM 7
#define BR_CTRL_BUS_NMI_BITNUM 8
#define BR_CTRL_BUS_BUSRQ_BITNUM 9
#define BR_CTRL_BUS_BUSACK_BITNUM 10

// Control bit masks
#define BR_CTRL_BUS_RD_MASK (1 << BR_CTRL_BUS_RD_BITNUM)
#define BR_CTRL_BUS_WR_MASK (1 << BR_CTRL_BUS_WR_BITNUM)
#define BR_CTRL_BUS_MREQ_MASK (1 << BR_CTRL_BUS_MREQ_BITNUM)
#define BR_CTRL_BUS_IORQ_MASK (1 << BR_CTRL_BUS_IORQ_BITNUM)
#define BR_CTRL_BUS_M1_MASK (1 << BR_CTRL_BUS_M1_BITNUM)
#define BR_CTRL_BUS_WAIT_MASK (1 << BR_CTRL_BUS_WAIT_BITNUM)
#define BR_CTRL_BUS_RESET_MASK (1 << BR_CTRL_BUS_RESET_BITNUM)
#define BR_CTRL_BUS_IRQ_MASK (1 << BR_CTRL_BUS_IRQ_BITNUM)
#define BR_CTRL_BUS_NMI_MASK (1 << BR_CTRL_BUS_NMI_BITNUM)
#define BR_CTRL_BUS_BUSRQ_MASK (1 << BR_CTRL_BUS_BUSRQ_BITNUM)
#define BR_CTRL_BUS_BUSACK_MASK (1 << BR_CTRL_BUS_BUSACK_BITNUM)

// Max length of code generated to execute a program
#define MAX_EXECUTOR_CODE_LEN 200
