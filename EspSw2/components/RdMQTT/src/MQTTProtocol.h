/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MQTT Protocol
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <vector>
#include <WString.h>

#include <functional>
// Put buffer callback function type
typedef std::function<void(const uint8_t *pBuf, unsigned bufLen)> MQTTPutFnType;
// Received frame callback function type
typedef std::function<void(const uint8_t *pBuf, unsigned bufLen)> MQTTFrameFnType;

class MQTTProtocolStats
{
public:
    MQTTProtocolStats()
    {
        clear();
    }
    void clear()
    {
        _rxFrameCount = 0;
        _frameCRCErrCount = 0;
        _frameTooLongCount = 0;
        _rxBufAllocFail = 0;
    }
    uint32_t _rxFrameCount;
    uint16_t _frameCRCErrCount;
    uint16_t _frameTooLongCount;
    uint16_t _rxBufAllocFail;
};

// MQTTProtocol
class MQTTProtocol
{
public:
    // Constructor for MQTTProtocol
    MQTTProtocol(MQTTPutFnType putFn, MQTTFrameFnType frameRxFn);

    // Destructor
    virtual ~MQTTProtocol();

    // Encode connect packets
    void encodeMQTTConnect(std::vector<uint8_t>& msgBuf, uint32_t keepAliveSecs, const char* clientID);
    void encodeMQTTPingReq(std::vector<uint8_t>& msgBuf);
    void encodeMQTTSubscribe(std::vector<uint8_t>& msgBuf, const char* topicFilter, uint32_t qos);
    void encodeMQTTPublish(std::vector<uint8_t>& msgBuf, const char* topicStr, const char* msgStr);

    // Check for connection ack
    bool checkForConnAck(const std::vector<uint8_t>& buf, bool& isError);

    // Handle new data
    void handleBuffer(const uint8_t* pBuf, unsigned bufLen);

    // Encode a frame into MQTT
    uint32_t encodeFrame(uint8_t* pEncoded, uint32_t maxEncodedLen, const uint8_t *pFrame, uint32_t frameLen);

    // Send a frame
    void sendFrame(const uint8_t *pBuf, unsigned bufLen);

    // Set frame rx max length
    void setFrameRxMaxLen(uint32_t rxMaxLen)
    {
        _rxBufMaxLen = rxMaxLen;
    }

    // Get frame rx max len
    uint32_t getFrameRxMaxLen()
    {
        return _rxBufMaxLen;
    }

    // Get frame tx buffer
    uint8_t* getFrameTxBuf()
    {
        return _txBuffer.data();
    }

    // Clear tx buffer
    void clearTxBuf()
    {
        _txBuffer.clear();
    }

    // Get frame tx len
    uint32_t getFrameTxLen()
    {
        return _txBuffer.size();
    }

    // Get stats
    const MQTTProtocolStats& getStats()
    {
        return _stats;
    }

    static const uint32_t MQTT_DEFAULT_FRAME_MAX_LEN = 1024;

private:

    // Callback functions
    MQTTPutFnType _putFn;
    MQTTFrameFnType _frameRxFn;

    // Packet ID
    uint16_t _packetID;

    // Receive buffer
    std::vector<uint8_t> _rxBuffer;
    uint16_t _rxBufMaxLen;

    // Transmit buffer
    std::vector<uint8_t> _txBuffer;

    // Stats
    MQTTProtocolStats _stats;

    // Helpers
    uint32_t bufferAppendString(std::vector<uint8_t>& buf, const char* pStr);
};
