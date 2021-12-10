/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolEndpointMsg
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <vector>
#include <RdJson.h>
#include <stdint.h>

// #define IMPLEMENT_PROTOCOL_MSG_JSON 1

static const uint32_t PROTOCOL_MSG_UNNUMBERED_NUM = UINT32_MAX;

enum ProtocolMsgProtocol 
{
    MSG_PROTOCOL_ROSSERIAL,
    MSG_PROTOCOL_M1SC,
    MSG_PROTOCOL_RICREST,
    MSG_PROTOCOL_NONE = 0x3f,
};

enum ProtocolMsgTypeCode
{
    MSG_TYPE_COMMAND,
    MSG_TYPE_RESPONSE,
    MSG_TYPE_PUBLISH,
    MSG_TYPE_REPORT
};

static const uint32_t MSG_CHANNEL_ID_ALL = 10000;

class ProtocolEndpointMsg
{
public:
    ProtocolEndpointMsg()
    {
        _channelID = 0;
        _msgProtocol = MSG_PROTOCOL_NONE;
        _msgNum = PROTOCOL_MSG_UNNUMBERED_NUM;
        _msgTypeCode = MSG_TYPE_REPORT;
    }

    ProtocolEndpointMsg(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgTypeCode msgTypeCode)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
    }

    void clear()
    {
        _cmdVector.clear();
        _cmdVector.shrink_to_fit();
    }

    void setFromBuffer(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgTypeCode msgTypeCode, const uint8_t* pBuf, uint32_t bufLen)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
        _cmdVector.assign(pBuf, pBuf+bufLen);

#ifdef IMPLEMENT_PROTOCOL_MSG_JSON
        setJSON();
#endif
    }

    void setFromBuffer(const uint8_t* pBuf, uint32_t bufLen)
    {
        _cmdVector.assign(pBuf, pBuf+bufLen);
    }

    void setBufferSize(uint32_t bufSize)
    {
        _cmdVector.resize(bufSize);
    }

    void setPartBuffer(uint32_t startPos, const uint8_t* pBuf, uint32_t len)
    {
        uint32_t reqSize = startPos + len;
        if (_cmdVector.size() < reqSize)
            _cmdVector.resize(reqSize);
        memcpy(_cmdVector.data() + startPos, pBuf, len);
    }

    void setProtocol(ProtocolMsgProtocol protocol)
    {
        _msgProtocol = protocol;
    }

    void setMsgTypeCode(ProtocolMsgTypeCode msgTypeCode)
    {
        _msgTypeCode = msgTypeCode;
    }

    void setAsResponse(const ProtocolEndpointMsg& reqMsg)
    {
        _channelID = reqMsg._channelID;
        _msgProtocol = reqMsg._msgProtocol;
        _msgNum = reqMsg._msgNum;
        _msgTypeCode = MSG_TYPE_RESPONSE;
    }

    void setAsResponse(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgTypeCode msgTypeCode)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgTypeCode = msgTypeCode;
    }

    ProtocolMsgProtocol getProtocol() const
    {
        return _msgProtocol;
    }

    ProtocolMsgTypeCode getMsgTypeCode()
    {
        return _msgTypeCode;
    }

    void setMsgNumber(uint32_t num)
    {
        _msgNum = num;
    }

    uint32_t getMsgNumber() const
    {
        return _msgNum;
    }

    void setChannelID(uint32_t channelID)
    {
        _channelID = channelID;
    }

    uint32_t getChannelID() const
    {
        return _channelID;
    }

    static const char* getProtocolAsString(ProtocolMsgProtocol msgProtocol)
    {
        switch(msgProtocol)
        {
            case MSG_PROTOCOL_ROSSERIAL: return "ROSSerial";
            case MSG_PROTOCOL_M1SC: return "Marty1SC";
            case MSG_PROTOCOL_RICREST: return "RICREST";
            default: return "None";
        }
    }

    static const char* getMsgTypeAsString(ProtocolMsgTypeCode msgTypeCode)
    {
        switch(msgTypeCode)
        {
            case MSG_TYPE_COMMAND: return "CMD";
            case MSG_TYPE_RESPONSE: return "RSP";
            case MSG_TYPE_PUBLISH: return "PUB";
            case MSG_TYPE_REPORT: return "REP";
            default:
                return "OTH";
        }
    }

#ifdef IMPLEMENT_PROTOCOL_MSG_JSON
    String getProtocolStr()
    {
        return getString("p", getProtocolAsString(MSG_PROTOCOL_NONE));
    }
    void setJSON()
    {
        _cmdJSON = String("{\"p\":\"") + getProtocolAsString(_msgProtocol) + String("\",") +
                    String("\"d\":\"") + getMsgTypeAsString(_msgTypeCode) + String("\",") +
                    String("\"n\":\"") + String(_msgNum) + String("\"}");
    }
    String getString(const char *dataPath, const char *defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue, _cmdJSON.c_str());
    }
    String getString(const char *dataPath, const String& defaultValue)
    {
        return RdJson::getString(dataPath, defaultValue.c_str(), _cmdJSON.c_str());
    }

    long getLong(const char *dataPath, long defaultValue)
    {
        return RdJson::getLong(dataPath, defaultValue, _cmdJSON.c_str());
    }

    double getDouble(const char *dataPath, double defaultValue)
    {
        return RdJson::getDouble(dataPath, defaultValue, _cmdJSON.c_str());
    }
#endif

    // Access to command vector
    const uint8_t* getBuf()
    {
        return _cmdVector.data();
    }
    uint32_t getBufLen()
    {
        return _cmdVector.size();
    }
    std::vector<uint8_t>& getCmdVector()
    {
        return _cmdVector;
    }

private:
    uint32_t _channelID;
    ProtocolMsgProtocol _msgProtocol;
    uint32_t _msgNum;
    ProtocolMsgTypeCode _msgTypeCode;
    std::vector<uint8_t> _cmdVector;
#ifdef IMPLEMENT_PROTOCOL_MSG_JSON
    String _cmdJSON;
#endif
};
