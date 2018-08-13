// Bus Raider
// Rob Dobson 2018

#pragma once

// #define DEBUG_SREC_RX 1

#include "globaldefs.h"

typedef void SrecHandlerDataCallback(uint32_t addr, const uint8_t* pData, uint32_t len);
typedef void SrecHandlerAddrCallback(uint32_t addr);

typedef enum {
    Srec_Ret_OK,
    Srec_Ret_LINE_COMPLETE,
    Srec_Ret_IGNORED,
    Srec_Ret_CHECKSUM_ERROR,
    Srec_Ret_INVALID_RECTYPE,
    Srec_Ret_INVALID_NYBBLE
} Srec_Ret;

// Init the destinations for SREC records
extern void srec_init();

extern void srec_decode(SrecHandlerDataCallback* pSrecDataCallback, SrecHandlerAddrCallback* pSrecAddrCallback,
			const uint8_t* pData, uint32_t len);
