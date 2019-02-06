// RemoteDebugProtocol 
// Rob Dobson 2019

#pragma once

#include <Arduino.h>
#include "AsyncTcp.h"
#include <vector>

typedef std::function<void(void* cbArg, const char *pData, size_t numChars)> RemoteDebugProtocolDataHandler;

class RemoteDebugProtocolServer;

class RemoteDebugProtocolSession
{
private:
    AsyncClient *_pClient;
    RemoteDebugProtocolServer *_pServer;

    void _onError(int8_t error);
    void _onAck(size_t len, uint32_t time);
    void _onDisconnect();
    void _onTimeout(uint32_t time);
    void _onData(void *buf, size_t len);
    void _onPoll();

public:
    RemoteDebugProtocolSession(RemoteDebugProtocolServer *pServer, AsyncClient *pClient);
    ~RemoteDebugProtocolSession();
    void forceClose();
    void sendChars(const char* pChars, int numChars);
};

class RemoteDebugProtocolServer
{
protected:
    // Server and active sessions list
    AsyncServer _server;
    std::vector<RemoteDebugProtocolSession*> _sessions;

    // Called by RemoteDebugProtocolSession on events
    void _handleDisconnect(RemoteDebugProtocolSession *pSess);
    void _handleData(const char* pData, int numChars);

    // Callback and argument for callback set in onData()
    RemoteDebugProtocolDataHandler _rxDataCallback;
    void* _rxDataCallbackArg;

public:
    static const int MAX_SESSIONS = 3;

    // Construction/Destruction
    RemoteDebugProtocolServer(int port);
    ~RemoteDebugProtocolServer();

    // Begin listening for sessions
    void begin();
    
    // Callback on data received from any session
    void onData(RemoteDebugProtocolDataHandler cb, void* arg = 0);

    // Send chars to all active sessions
    void sendChars(const char* pChars, int numChars);

    friend class RemoteDebugProtocolSession;
};
