// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once

#include "bare_metal_pi_zero.h"
#include "globaldefs.h"

#define DEBUG_SREC_RX 1

#ifdef __cplusplus
extern "C" {
#endif

typedef void TCmdHandlerDataBlockCallback(uint32_t addr, uint8_t* pData, uint32_t len, int type);

typedef enum {
    CMDHANDLER_RET_OK,
    CMDHANDLER_RET_LINE_COMPLETE,
    CMDHANDLER_RET_IGNORED,
    CMDHANDLER_RET_CHECKSUM_ERROR,
    CMDHANDLER_RET_INVALID_RECTYPE,
    CMDHANDLER_RET_INVALID_NYBBLE
} CmdHandler_Ret;

// Init the destinations for SREC and TREC records
extern void cmdHandler_init(TCmdHandlerDataBlockCallback* dataBlockCallback);

// Handle a single char
extern CmdHandler_Ret cmdHandler_handle_char(int ch);

// Error handling
extern void cmdHandler_clearError();
extern int cmdHandler_getError();

#ifdef __cplusplus
}
#endif