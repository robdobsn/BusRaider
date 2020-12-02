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
#include <Utils.h>

#define DEBUG_RICREST_MSG

// Logging
#ifdef DEBUG_RICREST_MSG
static const char* MODULE_PREFIX = "RICRESTMsg";
#endif

bool RICRESTMsg::decode(const uint8_t* pBuf, uint32_t len)
{
    // Debug
#ifdef DEBUG_RICREST_MSG
    String decodeMsgHex;
    Utils::getHexStrFromBytes(pBuf, len, decodeMsgHex);
    LOG_I(MODULE_PREFIX, "decode payloadLen %d payload %s", len, decodeMsgHex.c_str());
#endif

    // Check there is a RESTElementCode
    if (len <= RICREST_ELEM_CODE_POS)
        return false;

    // Extract RESTElementCode
    _RICRESTElemCode = (RICRESTElemCode)pBuf[RICREST_ELEM_CODE_POS];

#ifdef DEBUG_RICREST_MSG
    LOG_I(MODULE_PREFIX, "decode elemCode %d", _RICRESTElemCode);
#endif

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
            // Find any null-terminator within len
            int terminatorFoundIdx = -1;
            for (uint32_t i = RICREST_COMMAND_FRAME_PAYLOAD_POS; i < len; i++)
            {
                if (pBuf[i] == 0)
                {
                    terminatorFoundIdx = i;
                    break;
                }
            }

            // Extract json
            getStringFromBuf(_payloadJson, pBuf, RICREST_COMMAND_FRAME_PAYLOAD_POS, 
                    terminatorFoundIdx < 0 ? len : terminatorFoundIdx, 
                    RICREST_MAX_PAYLOAD_LEN);

            // Check for any binary element
            if (terminatorFoundIdx >= 0)
            {
                if (len > terminatorFoundIdx)
                {
                    _pBinaryData = pBuf + terminatorFoundIdx + 1;
                    _binaryLen = len - terminatorFoundIdx - 1;
                }
            }

#ifdef DEBUG_RICREST_MSG
            LOG_I(MODULE_PREFIX, "RICREST_CMD_FRAME json %s binaryLen %d", _payloadJson.c_str(), _binaryLen);
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
