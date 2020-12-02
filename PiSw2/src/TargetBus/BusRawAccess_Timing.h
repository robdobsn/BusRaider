// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Timeouts
static const int MAX_WAIT_FOR_PENDING_ACTION_US = 100000;
static const int MAX_WAIT_FOR_CTRL_LINES_COUNT = 10;
static const int MAX_WAIT_FOR_CTRL_BUS_VALID_US = 10;
static const int MIN_LOOP_COUNT_FOR_CTRL_BUS_VALID = 10000;

// Period target write control bus line is asserted during a write
static const int CYCLES_DELAY_FOR_WRITE_TO_TARGET = 250;

// Delay for settling of high address lines on PIB read
// TODO 2020 was 100
static const int CYCLES_DELAY_FOR_HIGH_ADDR_READ = 1000;

// Period target read control bus line is asserted during a read from the PIB (any bus element)
static const int CYCLES_DELAY_FOR_READ_FROM_PIB = 500;

// Max wait for end of read cycle
// TODO 2020 was 10
static const int MAX_WAIT_FOR_END_OF_READ_US = 100;

// Max wait for WAIT cycle start in debug mode
static const int MAX_WAIT_FOR_DEBUG_WAIT_US = 10000;

// Max wait for BUSACK start in debug mode
static const int MAX_WAIT_FOR_DEBUG_BUSACK_US = 10000;

// Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
// TODO 2020 following 3 were 15
static const int CYCLES_DELAY_FOR_CLEAR_LOW_ADDR = 500;
static const int CYCLES_DELAY_FOR_CLOCK_LOW_ADDR = 500;
static const int CYCLES_DELAY_FOR_LOW_ADDR_SET = 500;
static const int CYCLES_REPEAT_FOR_CLOCK_LOW_ADDR = 1;
// TODO 2020 was 20
static const int CYCLES_DELAY_FOR_HIGH_ADDR_SET = 1000;
static const int CYCLES_DELAY_FOR_WAIT_CLEAR = 50;
static const int CYCLES_DELAY_FOR_MREQ_FF_RESET = 20;
static const int CYCLES_DELAY_FOR_DATA_DIRN = 20;
static const int CYCLES_DELAY_FOR_TARGET_READ = 100;
// TODO 2020 was 10 - optimize
static const int CYCLES_DELAY_FOR_OUT_FF_SET = 1000;

// Delay in machine cycles for M1 to settle
static const int CYCLES_DELAY_FOR_M1_SETTLING = 100;