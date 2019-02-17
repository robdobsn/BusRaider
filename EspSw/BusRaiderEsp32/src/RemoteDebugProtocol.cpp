// RemoteDebugProtocol 
// Rob Dobson 2018

#include "RemoteDebugProtocol.h"
#include "ArduinoLog.h"

static const char* RemoteDebugProtocolWelcomeMessage = "Welcome to BusRaider\ncommand> ";
static const char* SESSION_PREFIX = "RemoteDebugProtocolSession: ";
static const char* MODULE_PREFIX = "RemoteDebugProtocolServer: ";

RemoteDebugProtocolSession::RemoteDebugProtocolSession(RemoteDebugProtocolServer *pServer, AsyncClient *pClient)
{
    _pServer = pServer;
    _pClient = pClient;
    Log.trace("%sConstructed\n", SESSION_PREFIX);
    pClient->onError([](void *sess, AsyncClient *c, int8_t error) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onError(error);
    }, this);
    pClient->onAck([](void *sess, AsyncClient *c, size_t len, uint32_t time) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onAck(len, time);
    }, this);
    pClient->onDisconnect([](void *sess, AsyncClient *c) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onDisconnect();
    }, this);
    pClient->onTimeout([](void *sess, AsyncClient *c, uint32_t time) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onTimeout(time);
    }, this);
    pClient->onData([](void *sess, AsyncClient *c, void *buf, size_t len) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onData(buf, len);
    }, this);
    pClient->onPoll([](void *sess, AsyncClient *c) {
        RemoteDebugProtocolSession *pSess = (RemoteDebugProtocolSession *)sess;
        pSess->_onPoll();
    }, this);

    // Send welcome message
    if (strlen(RemoteDebugProtocolWelcomeMessage) > 0)
        sendChars(RemoteDebugProtocolWelcomeMessage, strlen(RemoteDebugProtocolWelcomeMessage));
}

RemoteDebugProtocolSession::~RemoteDebugProtocolSession()
{
    Log.trace("%sDestructor", SESSION_PREFIX);
    // Delete the client
    if (_pClient)
    {
        delete _pClient;
        Log.trace("%sTCP client deleted heap %d pClient %d\n", SESSION_PREFIX, ESP.getFreeHeap(), int(_pClient));
    }
    _pClient = NULL;
}

void RemoteDebugProtocolSession::_onError(int8_t error)
{
    Log.trace("%sError %d\n", SESSION_PREFIX, error);
}

void RemoteDebugProtocolSession::_onAck(size_t len, uint32_t time)
{
    Log.verbose("%sAck len %d time %d\n", SESSION_PREFIX, len, time);
}

void RemoteDebugProtocolSession::_onDisconnect()
{
    Log.trace("%sdisconnected", SESSION_PREFIX);
    _pServer->_handleDisconnect(this);
}

void RemoteDebugProtocolSession::_onTimeout(uint32_t time)
{
    Log.trace("%sTimeout %d\n", SESSION_PREFIX, time);
}

void RemoteDebugProtocolSession::_onData(void *buf, size_t len)
{
    char debugStr[100+1];
    strlcpy(debugStr, (char*)buf, len);
    Log.verbose("%sonData %s\n", SESSION_PREFIX, debugStr);
    if (_pServer)
        _pServer->_handleData((const char*)buf, len);
}

void RemoteDebugProtocolSession::_onPoll()
{
    Log.verbose("%sPoll heap free %d\n", SESSION_PREFIX, xPortGetFreeHeapSize());
}

void RemoteDebugProtocolSession::forceClose()
{
    if (_pClient)
    {
        _pClient->close(true);
        _pClient->free();
    }
}

void RemoteDebugProtocolSession::sendChars(const char* pChars, int numChars)
{
    // Check if session open
    if (_pClient && _pClient->connected() && _pClient->canSend())
    {
        _pClient->write(pChars, numChars);
    }
}

RemoteDebugProtocolServer::RemoteDebugProtocolServer(int port) : _server(port)
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
        // Serial.printf("RemoteDebugProtocol Session created heap %d pSess %d\n", ESP.getFreeHeap(), int(c));

        // c->close(true);
        // c->free();
        // delete c;
        // return;


        // Wrap up the client in the session
        RemoteDebugProtocolSession *pRemoteDebugProtocolSession = new RemoteDebugProtocolSession((RemoteDebugProtocolServer *)s, c);

        heap_caps_check_integrity_all(true);

        // Get free session index
        int freeSessionIdx = -1;
        for (int i = 0; i < MAX_SESSIONS; i++)
        {
            if (_sessions[i] == NULL)
            {
                _sessions[i] = pRemoteDebugProtocolSession;
                freeSessionIdx = i;
                break;
            }
        }
        if ((pRemoteDebugProtocolSession == NULL) || (freeSessionIdx == -1))
        {
            c->close(true);
            c->free();
            delete c;
            if (pRemoteDebugProtocolSession == NULL)
                Log.warning("%sUnable to allocate mem for RemoteDebugProtocolSession", MODULE_PREFIX);
            else
                Log.trace("%sToo many sessions already", MODULE_PREFIX);
        }
        else
        {
            Log.trace("%sSession created heap %d pClient %d pSess %d\n", MODULE_PREFIX, ESP.getFreeHeap(), int(c), int(pRemoteDebugProtocolSession));
        }
    }, this);
}

RemoteDebugProtocolServer::~RemoteDebugProtocolServer()
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

void RemoteDebugProtocolServer::_handleDisconnect(RemoteDebugProtocolSession *pSess)
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
            Log.trace("%sSession deleted heap %d pSess %d\n", MODULE_PREFIX, ESP.getFreeHeap(), int(pSess));
        }
    }
    if (!deleted)
    {
        Log.trace("%sCan't delete session pSess %d - not found\n", MODULE_PREFIX, int(pSess));
    }
}

void RemoteDebugProtocolServer::_handleData(const char* pData, int numChars)
{
    // Log.trace("%sdata rx %d\n", MODULE_PREFIX, numChars);

    // Check if a callback is set
    if (_rxDataCallback)
    {
        _rxDataCallback(_rxDataCallbackArg, pData, numChars);
    }
}

void RemoteDebugProtocolServer::begin()
{
    _server.begin();
}

void RemoteDebugProtocolServer::sendChars(const char* pChars, int numChars)
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

void RemoteDebugProtocolServer::onData(RemoteDebugProtocolDataHandler cb, void* arg)
{
    _rxDataCallback = cb;
    _rxDataCallbackArg = arg;
}