// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once

#include "bare_metal_pi_zero.h"

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

extern void cmdHandler_init(uint32_t sRecBase, int sRecBufMaxLen, uint8_t* pTRecBasePtr, int tRecBufMaxLen);

// Handle a single char
extern CmdHandler_Ret cmdHandler_handle_char(int ch);

#ifdef __cplusplus
}
#endif