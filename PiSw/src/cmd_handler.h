// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once

#include "bare_metal_pi_zero.h"
#include "globaldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void cmdHandler_changeMachineCallbackType(const char* machineName);

extern void cmdHandler_init(cmdHandler_changeMachineCallbackType* pChangeMcCallback);

extern void cmdHandler_service();

// Handle a single char
extern void cmdHandler_handle_char(int ch);

#ifdef __cplusplus
}
#endif
