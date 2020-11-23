/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RICRESTMsg
// Message encapsulation for REST message
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RICRESTMsg.h"
#include <ProtocolEndpointMsg.h>
#include <stdio.h>

#define DEBUG_RICREST_MSG

// Logging
#ifdef DEBUG_RICREST_MSG
static const char* MODULE_PREFIX = "RICRESTMsg";
#endif

bool RICRESTMsg::decode(const uint8_t* pBuf, uint32_t len)
{
    // Debug
    // uint32_t sz = len;
    // const uint8_t* pVals = pBuf;
    // char outBuf[400];
    // strcpy(outBuf, "");
    // char tmpBuf[10];
    // for (int i = 0; i < sz; i++)
    // {
    //     sprintf(tmpBuf, "%02x ", pVals[i]);
    //     strlcat(outBuf, tmpBuf, sizeof(outBuf));
    // }

    // Debug
    // LOG_I(MODULE_PREFIX, "decode payloadLen %d payload %s", sz, outBuf);

    // Check there is a RESTElementCode
    if (len <= RICREST_ELEM_CODE_POS)
        return false;

    // Extract RESTElementCode
    _RICRESTElemCode = (RICRESTElemCode)pBuf[RICREST_ELEM_CODE_POS];
    switch(_RICRESTElemCode)
    {
        case RICREST_ELEM_CODE_URL:
        {
            // Set request
            getStringFromBuf(_req, pBuf, RICREST_HEADER_PAYLOAD_POS, len, RICREST_MAX_PAYLOAD_LEN);

            // Debug
#ifdef DEBUG_RICREST_MSG
            LOG_I(MODULE_PREFIX, "request %s", _req.c_str());
#endif
            break;
        }
        case RICREST_ELEM_CODE_CMDRESPJSON:
        {
            getStringFromBuf(_payloadJson, pBuf, RICREST_HEADER_PAYLOAD_POS, len, RICREST_MAX_PAYLOAD_LEN);
            _req = "resp";
            break;
        }
        case RICREST_ELEM_CODE_BODY:
        {
            // Extract params
            uint32_t offset = RICREST_BODY_BUFFER_POS;
            _bufferPos = uint32From(pBuf, offset, len);
            _totalBytes = uint32From(pBuf, offset, len);
            if (_totalBytes > MAX_REST_BODY_SIZE)
                _totalBytes = MAX_REST_BODY_SIZE;
            if (_bufferPos > _totalBytes)
                _bufferPos = 0;
            if (offset < len)
            {
                _pBinaryData = pBuf + offset;
                _binaryLen = len-offset;
            }
            _req = "elemBody";
            break;
        }
        case RICREST_ELEM_CODE_COMMAND_FRAME:
        {
            // Extract params
            getStringFromBuf(_payloadJson, pBuf, RICREST_COMMAND_FRAME_PAYLOAD_POS, len, RICREST_MAX_PAYLOAD_LEN);  
#ifdef DEBUG_RICREST_MSG
            LOG_I(MODULE_PREFIX, "RICREST CmdFrame %s", _payloadJson.c_str());
#endif
            _req = RdJson::getString("cmdName", "unknown", _payloadJson.c_str());          
            break;
        }
        case RICREST_ELEM_CODE_FILEBLOCK:
        {
            // Extract params
            uint32_t offset = RICREST_FILEBLOCK_FILE_POS;
            _bufferPos = uint32From(pBuf, offset, len);
            if (offset < len)
            {
                _pBinaryData = pBuf + offset;
                _binaryLen = len-offset;
            }
            _req = "ufBlock";
            break;
        }
        default:
            return false;
    }
    return true;
}

void RICRESTMsg::encode(const String& payload, ProtocolEndpointMsg& endpointMsg, RICRESTElemCode elemCode)
{
    // Setup buffer for the RESTElementCode
    uint8_t msgPrefixBuf[RICREST_HEADER_PAYLOAD_POS];
    msgPrefixBuf[RICREST_ELEM_CODE_POS] = elemCode;

    // Set the response ensuring to include the string terminator although this isn't stricly necessary
    endpointMsg.setBufferSize(RICREST_HEADER_PAYLOAD_POS + payload.length() + 1);
    endpointMsg.setPartBuffer(RICREST_ELEM_CODE_POS, msgPrefixBuf, sizeof(msgPrefixBuf));
    endpointMsg.setPartBuffer(RICREST_HEADER_PAYLOAD_POS, (uint8_t*)payload.c_str(), payload.length() + 1);
}

void RICRESTMsg::encode(const uint8_t* pBuf, uint32_t len, ProtocolEndpointMsg& endpointMsg, RICRESTElemCode elemCode)
{
    // Setup buffer for the RESTElementCode
    uint8_t msgPrefixBuf[RICREST_HEADER_PAYLOAD_POS];
    msgPrefixBuf[RICREST_ELEM_CODE_POS] = elemCode;

    // Set the response
    endpointMsg.setBufferSize(RICREST_HEADER_PAYLOAD_POS + len);
    endpointMsg.setPartBuffer(RICREST_ELEM_CODE_POS, msgPrefixBuf, sizeof(msgPrefixBuf));
    endpointMsg.setPartBuffer(RICREST_HEADER_PAYLOAD_POS, pBuf, len);
}
