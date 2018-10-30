// AsyncTelnetServer 
// Rob Dobson 2018

#include "AsyncTelnetServer.h"
#include "ArduinoLog.h"

AsyncTelnetSession::AsyncTelnetSession(AsyncTelnetServer *pServer, AsyncClient *pClient)
{
    _pServer = pServer;
    _pClient = pClient;
    Log.trace("AsyncTelnetSession: Constructed");
    pClient->onError([](void *sess, AsyncClient *c, int8_t error) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onError(error);
    }, this);
    pClient->onAck([](void *sess, AsyncClient *c, size_t len, uint32_t time) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onAck(len, time);
    }, this);
    pClient->onDisconnect([](void *sess, AsyncClient *c) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onDisconnect();
    }, this);
    pClient->onTimeout([](void *sess, AsyncClient *c, uint32_t time) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onTimeout(time);
    }, this);
    pClient->onData([](void *sess, AsyncClient *c, void *buf, size_t len) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onData(buf, len);
    }, this);
    pClient->onPoll([](void *sess, AsyncClient *c) {
        AsyncTelnetSession *pSess = (AsyncTelnetSession *)sess;
        pSess->_onPoll();
    }, this);
}

AsyncTelnetSession::~AsyncTelnetSession()
{
    Log.trace("AsyncTelnetSession: Destructor");
    // Delete the client
    if (_pClient)
    {
        delete _pClient;
        Log.trace("AsyncTelnetSession: TCP client deleted heap %d pClient %d\n", ESP.getFreeHeap(), int(_pClient));
    }
    _pClient = NULL;
}

void AsyncTelnetSession::_onError(int8_t error)
{
    Log.trace("AsyncTelnetSession: Error %d\n", error);
}

void AsyncTelnetSession::_onAck(size_t len, uint32_t time)
{
    Log.verbose("AsyncTelnetSession: Ack len %d time %d\n", len, time);
}

void AsyncTelnetSession::_onDisconnect()
{
    Log.trace("AsyncTelnetSession: disconnected");
    _pServer->_handleDisconnect(this);
}

void AsyncTelnetSession::_onTimeout(uint32_t time)
{
    Log.trace("AsyncTelnetSession: Timeout %d\n", time);
}

void AsyncTelnetSession::_onData(void *buf, size_t len)
{
    Log.verbose("AsyncTelnetSession: Data len %d\n", len);
    if (_pServer)
        _pServer->_handleData((const char*)buf, len);
}

void AsyncTelnetSession::_onPoll()
{
    Log.verbose("AsyncTelnetSession: Poll heap free %d\n", xPortGetFreeHeapSize());
}

void AsyncTelnetSession::forceClose()
{
    if (_pClient)
    {
        _pClient->close(true);
        _pClient->free();
    }
}

void AsyncTelnetSession::sendChars(const char* pChars, int numChars)
{
    // Check if session open
    if (_pClient && _pClient->connected() && _pClient->canSend())
    {
        _pClient->write(pChars, numChars);
    }
}

AsyncTelnetServer::AsyncTelnetServer(int port) : _server(port)
{
    // Init
    _rxDataCallback = NULL;
    _rxDataCallbackArg = NULL;

    // Create sessions list
    _sessions.resize(MAX_SESSIONS);
    for (int i = 0; i < MAX_SESSIONS; i++)
        _sessions[i] = NULL;

    // Handle new clients
    _server.onClient([this](void *s, AsyncClient *c) {

        if (c == NULL)
            return;
        // c->setRxTimeout(1);

        // // Close and delete immediately FOR TESTING
        // Serial.printf("Telnet Session created heap %d pSess %d\n", ESP.getFreeHeap(), int(c));

        // c->close(true);
        // c->free();
        // delete c;
        // return;


        // Wrap up the client in the telnet session
        AsyncTelnetSession *pAsyncTelnetSession = new AsyncTelnetSession((AsyncTelnetServer *)s, c);

        heap_caps_check_integrity_all(true);

        // Get free session index
        int freeSessionIdx = -1;
        for (int i = 0; i < MAX_SESSIONS; i++)
        {
            if (_sessions[i] == NULL)
            {
                _sessions[i] = pAsyncTelnetSession;
                freeSessionIdx = i;
                break;
            }
        }
        if ((pAsyncTelnetSession == NULL) || (freeSessionIdx == -1))
        {
            c->close(true);
            c->free();
            delete c;
            if (pAsyncTelnetSession == NULL)
                Log.warning("AsyncTelnetServer: Unable to allocate mem for AsyncTelnetSession");
            else
                Log.trace("AsyncTelnetServer: Too many sessions already");
        }
        else
        {
            Log.trace("AsyncTelnetServer: Session created heap %d pClient %d pSess %d\n", ESP.getFreeHeap(), int(c), int(pAsyncTelnetSession));
        }
    }, this);
}

AsyncTelnetServer::~AsyncTelnetServer()
{
    // Close and delete all remaining sessions
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (_sessions[i] != NULL)
        {
            // Close
            _sessions[i]->forceClose();
            // Delete session
            delete _sessions[i];
        }
    }    
}

void AsyncTelnetServer::_handleDisconnect(AsyncTelnetSession *pSess)
{
    // Find the session
    bool deleted = false;
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (_sessions[i] == pSess)
        {
            _sessions[i] = NULL;
            // Delete session
            delete pSess;
            deleted = true;
            Log.trace("AsyncTelnetSession: Session deleted heap %d pSess %d\n", ESP.getFreeHeap(), int(pSess));
        }
    }
    if (!deleted)
    {
        Log.trace("AsyncTelnetSession: Can't delete session pSess %d - not found\n", int(pSess));
    }
}

void AsyncTelnetServer::_handleData(const char* pData, int numChars)
{
    // Check if a callback is set
    if (_rxDataCallback)
    {
        _rxDataCallback(_rxDataCallbackArg, pData, numChars);
    }
}

void AsyncTelnetServer::begin()
{
    _server.begin();
}

void AsyncTelnetServer::sendChars(const char* pChars, int numChars)
{
    // For each currently active connection
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (_sessions[i])
        {
            _sessions[i]->sendChars(pChars, numChars);
        }
    }    
}

void AsyncTelnetServer::onData(AsyncTelnetDataHandler cb, void* arg)
{
    _rxDataCallback = cb;
    _rxDataCallbackArg = arg;
}
