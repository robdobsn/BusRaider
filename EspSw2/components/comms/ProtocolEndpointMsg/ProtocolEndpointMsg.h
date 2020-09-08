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

enum ProtocolMsgDirection
{
    MSG_DIRECTION_COMMAND,
    MSG_DIRECTION_RESPONSE,
    MSG_DIRECTION_PUBLISH,
    MSG_DIRECTION_REPORT
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
        _msgDirection = MSG_DIRECTION_REPORT;
    }

    ProtocolEndpointMsg(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgDirection msgDirection)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgDirection = msgDirection;
    }

    void setFromBuffer(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgDirection msgDirection, const uint8_t* pBuf, uint32_t bufLen)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgDirection = msgDirection;
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

    void setDirection(ProtocolMsgDirection direction)
    {
        _msgDirection = direction;
    }

    void setAsResponse(const ProtocolEndpointMsg& reqMsg)
    {
        _channelID = reqMsg._channelID;
        _msgProtocol = reqMsg._msgProtocol;
        _msgNum = reqMsg._msgNum;
        _msgDirection = MSG_DIRECTION_RESPONSE;
    }

    void setAsResponse(uint32_t channelID, ProtocolMsgProtocol msgProtocol, uint32_t msgNum, ProtocolMsgDirection msgDirn)
    {
        _channelID = channelID;
        _msgProtocol = msgProtocol;
        _msgNum = msgNum;
        _msgDirection = msgDirn;
    }

    ProtocolMsgProtocol getProtocol()
    {
        return _msgProtocol;
    }

    ProtocolMsgDirection getDirection()
    {
        return _msgDirection;
    }

    void setMsgNumber(uint32_t num)
    {
        _msgNum = num;
    }

    uint32_t getMsgNumber()
    {
        return _msgNum;
    }

    void setChannelID(uint32_t channelID)
    {
        _channelID = channelID;
    }

    uint32_t getChannelID()
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

    static const char* getDirectionAsString(ProtocolMsgDirection msgDirection)
    {
        switch(msgDirection)
        {
            case MSG_DIRECTION_COMMAND: return "CMD";
            case MSG_DIRECTION_RESPONSE: return "RSP";
            case MSG_DIRECTION_PUBLISH: return "PUB";
            case MSG_DIRECTION_REPORT: return "REP";
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
                    String("\"d\":\"") + getDirectionAsString(_msgDirection) + String("\",") +
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
    ProtocolMsgDirection _msgDirection;
    std::vector<uint8_t> _cmdVector;
#ifdef IMPLEMENT_PROTOCOL_MSG_JSON
    String _cmdJSON;
#endif
};
