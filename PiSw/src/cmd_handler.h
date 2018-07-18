// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once

#include "globaldefs.h"
#include "bare_metal_pi_zero.h"

#define DEBUG_SREC_RX 1

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	CMDHANDLER_RET_OK,
	CMDHANDLER_RET_IGNORED,
	CMDHANDLER_RET_CHECKSUM_ERROR,
	CMDHANDLER_RET_LINE_COMPLETE
} CmdHandler_Ret;

// Init the destinations for SREC and TREC records
extern void cmdHandler_init(uint8_t* pSRecBase, int sRecBufMaxLen, uint8_t* pTRecBase, int tRecBufMaxLen);

// Handle a single char
extern CmdHandler_Ret cmdHandler_handle_char(int ch);

#ifdef DEBUG_SREC_RX
extern int cmdHandler_isError();
extern char* cmdHandler_getError();
extern void cmdHandler_errorClear();
#endif

#ifdef __cplusplus
}
#endif