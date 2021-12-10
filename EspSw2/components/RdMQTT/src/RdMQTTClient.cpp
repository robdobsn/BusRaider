/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdMQTTClient
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdMQTTClient.h"
#include "Utils.h"
#include "ArduinoTime.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include <vector>

// Debug
// #define DEBUG_MQTT_CONNECTION
// #define DEBUG_SEND_DATA
// #define DEBUG_MQTT_CLIENT_RX

// Log prefix
static const char *MODULE_PREFIX = "MQTTClient";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdMQTTClient::RdMQTTClient() :
            _mqttProtocol(
                std::bind(&RdMQTTClient::putDataCB, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&RdMQTTClient::frameRxCB, this, std::placeholders::_1, std::placeholders::_2))
{
    // Settings
    _isEnabled = false;
    _brokerPort = DEFAULT_MQTT_PORT;
    _keepAliveSecs = MQTT_DEFAULT_KEEPALIVE_TIME_SECS;

    // Vars
    _connState = MQTT_STATE_DISCONNECTED;
    _clientHandle = 0;
    _lastConnStateChangeMs = 0;
    _rxFrameMaxLen = MQTT_DEFAULT_FRAME_MAX_LEN;
    _lastKeepAliveMs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdMQTTClient::~RdMQTTClient()
{
    disconnect();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::setup(bool isEnabled, const char *brokerHostname, uint32_t brokerPort, const char* clientID)
{
    // Disconnect if required
    disconnect();

    // Clear list of topics
    _topicList.clear();

    // Store settings
    _isEnabled = isEnabled;
    _brokerHostname = brokerHostname;
    _brokerPort = brokerPort;
    _clientID = clientID;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add topic
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::addTopic(const char* topicName, bool isInbound, const char* topicFilter, uint8_t qos)
{
    // Create record
    TopicInfo topicInfo = {topicName, isInbound, topicFilter, qos};
    _topicList.push_back(topicInfo);
    LOG_I(MODULE_PREFIX, "addTopic name %s isInbound %s path %s qos %d", topicName, isInbound ? "Y" : "N", topicFilter, qos);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get topic names
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::getTopicNames(std::vector<String>& topicNames, bool includeInbound, bool includeOutbound)
{
    // Get names of topics
    topicNames.clear();
    for (TopicInfo& topicInfo : _topicList)
    {
        if (includeInbound && topicInfo.isInbound)
            topicNames.push_back(topicInfo.topicName);
        if (includeOutbound && !topicInfo.isInbound)
            topicNames.push_back(topicInfo.topicName);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::service()
{
    // Check if enabled
    if (!_isEnabled)
        return;

    // Handle connection state
    bool isError = false;
    bool connClosed = false;
    switch (_connState)
    {
        case MQTT_STATE_DISCONNECTED:
        {
            // See if it's time to try to connect
            if (Utils::isTimeout(millis(), _lastConnStateChangeMs, MQTT_RETRY_CONNECT_TIME_MS))
            {
                // Don't try again immediately
                _lastConnStateChangeMs = millis();

                // Try to establish connection
                socketConnect();
            }
            break;
        }
        case MQTT_STATE_SOCK_CONN:
        {
            // Send CONNECT packet
            std::vector<uint8_t> msgBuf;
            _mqttProtocol.encodeMQTTConnect(msgBuf, _keepAliveSecs, _clientID.c_str());

            // Send packet
            sendTxData(msgBuf, isError, connClosed);
            _connState = MQTT_STATE_MQTT_CONN_SENT;
            _lastConnStateChangeMs = millis();
            break;
        }
        case MQTT_STATE_MQTT_CONN_SENT:
        {
            // Check for data
            std::vector<uint8_t> rxData;
            bool dataRxOk = getRxData(rxData, isError, connClosed);
            if (dataRxOk)
            {
                // Handle rx data
                String rxDataStr;
                Utils::getHexStrFromBytes(rxData.data(), rxData.size(), rxDataStr);
#ifdef DEBUG_MQTT_CLIENT_RX
                LOG_I(MODULE_PREFIX, "service rx %s", rxDataStr.c_str());
#endif

                // Check response
                if (_mqttProtocol.checkForConnAck(rxData, isError))
                {
                    // Perform subscriptions
                    subscribeToTopics();

                    // Now connected
                    _connState = MQTT_STATE_MQTT_CONNECTED;
                    _lastConnStateChangeMs = millis();
                    _lastKeepAliveMs = millis();
                }
            }
            break;
        }
        case MQTT_STATE_MQTT_CONNECTED:
        {
            // Check for keep-alive
            if (Utils::isTimeout(millis(), _lastKeepAliveMs, _keepAliveSecs * 500))
            {
                // Send PINGREQ packet
                std::vector<uint8_t> msgBuf;
                _mqttProtocol.encodeMQTTPingReq(msgBuf);

                // Send packet
                sendTxData(msgBuf, isError, connClosed);
                _lastKeepAliveMs = millis();
            }

            // Check for data
            std::vector<uint8_t> rxData;
            bool dataRxOk = getRxData(rxData, isError, connClosed);
            if (dataRxOk)
            {
                // Handle rx data
                String rxDataStr;
                Utils::getHexStrFromBytes(rxData.data(), rxData.size(), rxDataStr);
#ifdef DEBUG_MQTT_CLIENT_RX
                LOG_I(MODULE_PREFIX, "service rx %s", rxDataStr.c_str());
#endif
            }
            break;
        }
    }

    // Error handling
    if (isError || connClosed)
    {
        if (isError)
            close(_clientHandle);
        // Conn closed so we'll need to retry sometime later
        _connState = MQTT_STATE_DISCONNECTED;
        _lastConnStateChangeMs = millis();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Deinit
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::disconnect()
{
    // Check if already connected
    if (_connState == MQTT_STATE_DISCONNECTED)
        return;

    // Close socket
    close(_clientHandle);
    _connState = MQTT_STATE_DISCONNECTED;
    _lastConnStateChangeMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Publish to topic
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdMQTTClient::publishToTopic(const String& topicName, const String& msgStr)
{
    // Find topic
    String topicFilter = "";
    for (TopicInfo& topicInfo : _topicList)
    {
        if (topicName.equals(topicInfo.topicName))
        {
            topicFilter = topicInfo.topicFilter;
            break;
        }
    }
    if (topicFilter.length() == 0)
        return false;

    // Form PUBLISH packet
    std::vector<uint8_t> msgBuf;
    _mqttProtocol.encodeMQTTPublish(msgBuf, topicFilter.c_str(), msgStr.c_str());

    // Send packet
    bool isError = false;
    bool connClosed = false;
    return sendTxData(msgBuf, isError, connClosed);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put data to interface callback function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::putDataCB(const uint8_t *pBuf, unsigned bufLen)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Frame rx callback function (frame received from interface)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::frameRxCB(const uint8_t *pBuf, unsigned bufLen)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Attempt to connect via socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::socketConnect()
{
#ifdef DEBUG_MQTT_CONNECTION
    LOG_I(MODULE_PREFIX, "socketConnect attempting to connect to %s port %d", _brokerHostname.c_str(), _brokerPort);
#endif

    // Get address info
    struct addrinfo addrInfo = {}, *pFoundAddrs = nullptr;
    addrInfo.ai_family = AF_INET;
    addrInfo.ai_socktype = SOCK_STREAM;
    addrInfo.ai_protocol = IPPROTO_TCP;
    String portStr = String(_brokerPort);
    int err = getaddrinfo(_brokerHostname.c_str(), portStr.c_str(), &addrInfo, &pFoundAddrs);
    if (err != 0)
    {
        LOG_I(MODULE_PREFIX, "socketConnect broker lookup failed %s error %d", _brokerHostname.c_str(), err);
        return;
    }
    for(struct addrinfo *pAddr = pFoundAddrs; pAddr != nullptr; pAddr = pAddr->ai_next)
    {
        _clientHandle = socket(pAddr->ai_family, pAddr->ai_socktype, pAddr->ai_protocol);
        if (_clientHandle == -1)
        {
            err = errno;
            LOG_I(MODULE_PREFIX, "socketConnect sock failed %s error %d", _brokerHostname.c_str(), err);
            break;
        }

        int connRslt = connect(_clientHandle, pAddr->ai_addr, pAddr->ai_addrlen);
        if (connRslt < 0)
        {
            String hexStr;
            Utils::getHexStrFromBytes((const uint8_t*)(pAddr->ai_addr), pAddr->ai_addrlen, hexStr);
            LOG_I(MODULE_PREFIX, "socketConnect conn failed %s error %d addrInfo %s", _brokerHostname.c_str(), errno, hexStr.c_str());
        }

        if (connRslt == 0)
        {
            LOG_I(MODULE_PREFIX, "socketConnect conn ok %s", _brokerHostname.c_str());
            _connState = MQTT_STATE_SOCK_CONN;
            break;
        }

        // Close socket if failed
        err = errno;
        close(_clientHandle);
    }

    // Clean-up
    freeaddrinfo(pFoundAddrs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Attempt to connect via socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdMQTTClient::getRxData(std::vector<uint8_t>& rxData, bool& isError, bool& connClosed)
{
    // Create data buffer
    isError = false;
    connClosed = false;
    uint8_t* pBuf = new uint8_t[_rxFrameMaxLen];
    if (!pBuf)
    {
        LOG_E(MODULE_PREFIX, "getRxData failed alloc");
        return false;
    }

    // Check for data
    int32_t bufLen = recv(_clientHandle, pBuf, _rxFrameMaxLen, MSG_DONTWAIT);

    // Handle error conditions
    if (bufLen < 0)
    {
        switch(errno)
        {
            case EWOULDBLOCK:
                bufLen = 0;
                break;
            default:
                LOG_W(MODULE_PREFIX, "getRxData read error %d", errno);
                isError = true;
                break;
        }
        delete [] pBuf;
        return false;
    }

    // Handle connection closed
    if (bufLen == 0)
    {
        LOG_W(MODULE_PREFIX, "getRxData conn closed %d", errno);
        connClosed = true;
        delete [] pBuf;
        return false;
    }

    // Return received data
    rxData.assign(pBuf, pBuf+bufLen);
    delete [] pBuf;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send data over socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdMQTTClient::sendTxData(std::vector<uint8_t>& txData, bool& isError, bool& connClosed)
{
    int rslt = send(_clientHandle, txData.data(), txData.size(), 0);
    connClosed = false;
    isError = false;
    if (rslt < 0)
    {
        LOG_W(MODULE_PREFIX, "sendTxData send error %d", errno);
        isError = true;
        return false;
    }

#ifdef DEBUG_SEND_DATA
    // Debug
    String txDataStr;
    Utils::getHexStrFromBytes(txData.data(), txData.size(), txDataStr);
    LOG_I(MODULE_PREFIX, "service tx %s", txDataStr.c_str());
#endif

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Subscribe to topics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdMQTTClient::subscribeToTopics()
{
    LOG_I(MODULE_PREFIX, "subscribeToTopics topics %d", _topicList.size());

    // Iterate list and subscribe to inbound topics
    for (TopicInfo& topicInfo : _topicList)
    {
        LOG_I(MODULE_PREFIX, "subscribeToTopics topic isInbound %d filter %s QoS %d", 
                topicInfo.isInbound, topicInfo.topicFilter.c_str(), topicInfo.qos);

        if (topicInfo.isInbound)
        {
            // Send CONNECT packet
            std::vector<uint8_t> msgBuf;
            _mqttProtocol.encodeMQTTSubscribe(msgBuf, topicInfo.topicFilter.c_str(), topicInfo.qos);

            // Send packet
            bool isError = false;
            bool connClosed = false;
            sendTxData(msgBuf, isError, connClosed);
        }
    }
}
