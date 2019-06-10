// RemoteDebugProtocol 
// Rob Dobson 2019

#pragma once

#include <Arduino.h>
#include "AsyncTCP.h"
#include <vector>

typedef std::function<void(void* cbArg, const uint8_t *pData, size_t dataLen)> RemoteDebugProtocolDataHandler;

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
    void sendChars(const uint8_t* pData, int dataLen);
};

class RemoteDebugProtocolServer
{
protected:
    // Server and active sessions list
    AsyncServer _server;
    std::vector<RemoteDebugProtocolSession*> _sessions;

    // Called by RemoteDebugProtocolSession on events
    void _handleDisconnect(RemoteDebugProtocolSession *pSess);
    void _handleData(const uint8_t* pData, int dataLen);

    // Callback and argument for callback set in onData()
    RemoteDebugProtocolDataHandler _rxDataCallback;
    void* _rxDataCallbackArg;

    // Pointer to welcome message
    const char* _pWelcomeMessage;

public:
    static const int MAX_SESSIONS = 3;

    // Construction/Destruction
    RemoteDebugProtocolServer(int port, const char* pWelcomeMessageMustBeStatic);
    ~RemoteDebugProtocolServer();

    // Begin listening for sessions
    void begin();
    
    // Callback on data received from any session
    void onData(RemoteDebugProtocolDataHandler cb, void* arg = 0);

    // Send chars to all active sessions
    void sendChars(const uint8_t* pData, int dataLen);

    friend class RemoteDebugProtocolSession;
};
