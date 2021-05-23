/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdint.h"
#include <functional>

enum RdWebSocketEventCode
{
    WEBSOCKET_EVENT_CONNECT,
    WEBSOCKET_EVENT_DISCONNECT_EXTERNAL, // other side disconnected
    WEBSOCKET_EVENT_DISCONNECT_INTERNAL, // we disconnected
    WEBSOCKET_EVENT_DISCONNECT_ERROR,    // error
    WEBSOCKET_EVENT_TEXT,
    WEBSOCKET_EVENT_BINARY,
    WEBSOCKET_EVENT_PING,
    WEBSOCKET_EVENT_PONG,
    WEBSOCKET_EVENT_NONE
};

// WebSocket opcodes
enum WebSocketOpCodes
{
    WEBSOCKET_OPCODE_CONTINUE = 0x0,
    WEBSOCKET_OPCODE_TEXT = 0x1,
    WEBSOCKET_OPCODE_BINARY = 0x2,
    WEBSOCKET_OPCODE_CLOSE = 0x8,
    WEBSOCKET_OPCODE_PING = 0x9,
    WEBSOCKET_OPCODE_PONG = 0xA
};

typedef std::function<void(RdWebSocketEventCode eventCode, const uint8_t* pBuf, uint32_t bufLen)> RdWebSocketCB;
