/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lwip/api.h"

static const int DEFAULT_HTTP_PORT = 80;

class RdWebServerSettings
{
public:
    RdWebServerSettings()
    {
        serverTCPPort = DEFAULT_HTTP_PORT;
        numConnSlots = 6;
        maxWebSockets = 3;
        pingIntervalMs = 1000;
    }

    RdWebServerSettings(int port, uint32_t connSlots, uint32_t maxWS, uint32_t pingIntervalMs)
    {
        serverTCPPort = port;
        numConnSlots = connSlots;
        maxWebSockets = maxWS;
        this->pingIntervalMs = pingIntervalMs;
    }

    // TCP port of server
    int serverTCPPort;

    // Number of connection slots
    uint32_t numConnSlots;

    // Max number of web sockets
    uint32_t maxWebSockets;

    // Ping interval for websockets (0 to disable)
    uint32_t pingIntervalMs;
};
