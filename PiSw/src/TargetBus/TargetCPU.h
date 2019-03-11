// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Return codes from wait-state ISR

// Indicate that memory or IO requested is not supported by the machine
#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x8000

// The returned value is an instruction to be injected into the target
// machine program flow - this is the mechanism used to set/get registers
#define BR_MEM_ACCESS_INSTR_INJECT 0x4000

// The target processor should be held at this point - for single-stepping
#define BR_MEM_ACCESS_HOLD 0x2000

// Control bus bits used to pass to machines, etc
#define BR_CTRL_BUS_RD 0
#define BR_CTRL_BUS_WR 1
#define BR_CTRL_BUS_MREQ 2
#define BR_CTRL_BUS_IORQ 3
#define BR_CTRL_BUS_M1 4
#define BR_CTRL_BUS_WAIT 5

// Control bit masks
#define BR_CTRL_BUS_RD_MASK (1 << BR_CTRL_BUS_RD)
#define BR_CTRL_BUS_WR_MASK (1 << BR_CTRL_BUS_WR)
#define BR_CTRL_BUS_MREQ_MASK (1 << BR_CTRL_BUS_MREQ)
#define BR_CTRL_BUS_IORQ_MASK (1 << BR_CTRL_BUS_IORQ)
#define BR_CTRL_BUS_M1_MASK (1 << BR_CTRL_BUS_M1)
#define BR_CTRL_BUS_WAIT_MASK (1 << BR_CTRL_BUS_WAIT)

typedef uint32_t BusAccessCBFnType(uint32_t addr, uint32_t data, uint32_t flags);
