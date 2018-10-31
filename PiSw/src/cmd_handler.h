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
typedef void cmdHandler_rxcharCallbackType(const uint8_t* rxChars, int rxLen);

extern void cmdHandler_init(cmdHandler_changeMachineCallbackType* pChangeMcCallback);

extern void cmdHandler_service();

// Handle a single char
extern void cmdHandler_handle_char(int ch);

// Status update
extern void cmdHandler_sendReqStatusUpdate();

// Send key
extern void cmdHandler_sendKeyCode(int keyCode);

// Set callback for received chars
extern void cmd_handler_set_rxchar_callback(cmdHandler_rxcharCallbackType* pRxCharCallback);

// Get ESP health info
extern void cmdHandler_getESPHealth(bool* espIPAddressValid, char** espIPAddress, char** espWifiConnStr, char** espWifiSSID);

#ifdef __cplusplus
}
#endif
