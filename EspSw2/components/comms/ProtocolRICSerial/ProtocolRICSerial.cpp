/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICSerial
// Protocol wrapper implementing RICSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ProtocolRICSerial.h"
#include <ProtocolEndpointMsg.h>
#include <ConfigBase.h>
#include <MiniHDLC.h>
#include <Utils.h>

// Logging
static const char* MODULE_PREFIX = "RICSerial";

// Warn
#define WARN_ON_NO_HDLC_HANDLER

// Debug
// #define DEBUG_PROTOCOL_RIC_SERIAL_DECODE
// #define DEBUG_PROTOCOL_RIC_SERIAL_DECODE_DETAIL
// #define DEBUG_PROTOCOL_RIC_SERIAL_ENCODE
// #define DEBUG_PROTOCOL_RIC_SERIAL_ENCODE_DETAIL

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolRICSerial::ProtocolRICSerial(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB) :
    ProtocolBase(channelID), _msgTxCB(msgTxCB), _msgRxCB(msgRxCB)
{
    // Extract configuration
    ConfigBase config(configJSON);
    _maxRxMsgLen = config.getLong("MaxRxMsgLen", DEFAULT_RIC_SERIAL_RX_MAX);
    _maxTxMsgLen = config.getLong("MaxTxMsgLen", DEFAULT_RIC_SERIAL_TX_MAX);
    unsigned frameBoundary = config.getLong("FrameBound", 0x7E);
    unsigned controlEscape = config.getLong("CtrlEscape", 0x7D);

    // New HDLC
    _pHDLC = new MiniHDLC(NULL, 
            std::bind(&ProtocolRICSerial::hdlcFrameRxCB, this, std::placeholders::_1, std::placeholders::_2),
            frameBoundary,
            controlEscape,
            _maxTxMsgLen, _maxRxMsgLen);

    // Debug
    LOG_I(MODULE_PREFIX, "constructor maxRxMsgLen %d maxTxMsgLen %d", _maxRxMsgLen, _maxTxMsgLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolRICSerial::~ProtocolRICSerial()
{
    // Clean up
    if (_pHDLC)
        delete _pHDLC;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// addRxData
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolRICSerial::addRxData(const uint8_t* pData, uint32_t dataLen)
{
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_DECODE
    // Debug
    LOG_I(MODULE_PREFIX, "addRxData len %d", dataLen);
#endif
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_DECODE_DETAIL
    String dataStr;
    Utils::getHexStrFromBytes(pData, dataLen, dataStr);
    LOG_I(MODULE_PREFIX, "addRxData %s", dataStr.c_str());
#endif

    // Valid?
    if (!_pHDLC)
    {
#ifdef WARN_ON_NO_HDLC_HANDLER
        LOG_W(MODULE_PREFIX, "addRxData no HDLC handler");
#endif
        return;
    }

    // Add data
    _pHDLC->handleBuffer(pData, dataLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// addRxData
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #define USE_OPTIMIZED_ENCODE_CAUSES_HEAP_FAULT

void ProtocolRICSerial::encodeTxMsgAndSend(ProtocolEndpointMsg& msg)
{
    // Add to HDLC
    if (_pHDLC)
    {
#ifdef USE_OPTIMIZED_ENCODE_CAUSES_HEAP_FAULT
        // Create the message header
        uint8_t msgHeader[2];
        msgHeader[0] = msg.getMsgNumber();
        uint8_t protocolDirnByte = ((msg.getMsgTypeCode() & 0x03) << 6) + (msg.getProtocol() & 0x3f);
        msgHeader[1] = protocolDirnByte;

        // Calculate encoded length of full message
        uint32_t encFrameLen = _pHDLC->calcEncodedPayloadLen(msgHeader, 2);
        encFrameLen += _pHDLC->calcEncodedPayloadLen(msg.getBuf(), msg.getBufLen());
        encFrameLen += _pHDLC->maxEncodedLen(0);

        // Check length is ok
        if (encFrameLen > _maxTxMsgLen)
        {
            LOG_E(MODULE_PREFIX, "encodeTxMsgAndSend encodedLen %d > max %d", encFrameLen, _maxTxMsgLen);
            return;
        }

        // Allocate buffer for the frame
        std::vector<uint8_t> ricSerialMsg;
        ricSerialMsg.resize(encFrameLen);

        // Encode start
        uint16_t fcs = 0;
        uint32_t curPos = _pHDLC->encodeFrameStart(ricSerialMsg.data(), ricSerialMsg.size(), fcs);

        // Encode header
        curPos = _pHDLC->encodeFrameAddPayload(ricSerialMsg.data(), ricSerialMsg.size(), fcs, curPos, msgHeader, sizeof(msgHeader));

        // Encode payload
        curPos = _pHDLC->encodeFrameAddPayload(ricSerialMsg.data(), ricSerialMsg.size(), fcs, curPos, msg.getBuf(), msg.getBufLen());

        // Complete encoding
        curPos = _pHDLC->encodeFrameEnd(ricSerialMsg.data(), ricSerialMsg.size(), fcs, curPos);

        if (curPos == 0)
        {
            LOG_E(MODULE_PREFIX, "encodeTxMsgAndSend unexpected encFrameLen %d > max %d", encFrameLen, _maxTxMsgLen);
            return;
        }
        else if (curPos != encFrameLen)
        {
            LOG_W(MODULE_PREFIX, "encodeTxMsgAndSend length %d != expectedLen %d", curPos, encFrameLen);
        }

        // Debug
        // LOG_I(MODULE_PREFIX, "encodeTxMsgAndSend, origLen %d encoded len %d", msg.getBufLen(), encFrameLen);

        // Set message into buffer
        msg.setFromBuffer(ricSerialMsg.data(), ricSerialMsg.size());
#else
        // Create the message
        std::vector<uint8_t> ricSerialMsg;
        ricSerialMsg.reserve(msg.getBufLen()+2);
        ricSerialMsg.push_back(msg.getMsgNumber());
        uint8_t protocolDirnByte = ((msg.getMsgTypeCode() & 0x03) << 6) + (msg.getProtocol() & 0x3f);
        ricSerialMsg.push_back(protocolDirnByte);
        ricSerialMsg.insert(ricSerialMsg.end(), msg.getCmdVector().begin(), msg.getCmdVector().end());
        _pHDLC->sendFrame(ricSerialMsg.data(), ricSerialMsg.size());
        msg.setFromBuffer(_pHDLC->getFrameTxBuf(), _pHDLC->getFrameTxLen());
        _pHDLC->clearTxBuf();
#endif

        // Debug
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_ENCODE
        LOG_I(MODULE_PREFIX, "encodeTxMsgAndSend, encoded len %d", msg.getBufLen());
#endif
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_ENCODE_DETAIL
        {
        String outStr;
        Utils::getHexStrFromBytes(msg.getBuf(), msg.getBufLen(), outStr);
        LOG_I(MODULE_PREFIX, "encodeTxMsgAndSend %s", outStr.c_str());
        }
#endif

        // Send
        _msgTxCB(msg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolRICSerial::hdlcFrameRxCB(const uint8_t* pFrame, int frameLen)
{
    // Check callback is valid
    if (!_msgRxCB)
        return;
    // Check validity of frame length
    if (frameLen < 2)
        return;

    // Extract message type
    uint32_t msgNumber = pFrame[0];
    uint32_t msgProtocolCode = pFrame[1] & 0x3f;
    uint32_t msgTypeCode = pFrame[1] >> 6;

    // Debug
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_DECODE
    LOG_I(MODULE_PREFIX, "hdleFrameRxCB len %d msgNum %d protocolCode %d msgTypeCode %d", 
                    frameLen, msgNumber, msgProtocolCode, msgTypeCode);
#endif
#ifdef DEBUG_PROTOCOL_RIC_SERIAL_DECODE_DETAIL
    String dataStr;
    Utils::getHexStrFromBytes(pFrame, frameLen, dataStr);
    LOG_I(MODULE_PREFIX, "hdleFrameRxCB %s", dataStr.c_str());
#endif

    // Convert to ProtocolEndpointMsg
    ProtocolEndpointMsg endpointMsg;
    endpointMsg.setFromBuffer(_channelID, (ProtocolMsgProtocol)msgProtocolCode, msgNumber, (ProtocolMsgTypeCode)msgTypeCode, pFrame+2, frameLen-2);
    _msgRxCB(endpointMsg);
}