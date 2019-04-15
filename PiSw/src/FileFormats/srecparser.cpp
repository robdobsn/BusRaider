// Bus Raider
// Rob Dobson 2018

#include "srecparser.h"
#include "../System/logging.h"
#include <stddef.h>

#define MAX_SREC_DATA_LEN 200

// States while decoding a line of Motorola SREC format
typedef enum {
    SREC_LINE_STATE_INIT,
    SREC_LINE_STATE_RECTYPE,
    SREC_LINE_STATE_LEN,
    SREC_LINE_STATE_ADDR,
    SREC_LINE_STATE_DATA,
    SREC_LINE_STATE_CHECKSUM
} Srec_LineState;

// Type of Motorola SREC record
typedef enum {
    SREC_RECTYPE_DATA,
    SREC_RECTYPE_START,
    SREC_RECTYPE_COUNT
} Srec_RecType;

// State struct
struct Srec_State {
	Srec_LineState lineState;
	Srec_RecType recType;
	uint32_t checksum;
	uint32_t entryAddr;
	int addrBytes;
	int fieldCtr;
	int byteCtr;
	int lineLen;
	int dataLen;
	int byteIdx;
	uint32_t addr;
	uint8_t byte;
	uint8_t data[MAX_SREC_DATA_LEN];
	int lastCharInvalid;
	int debugChCount;
	Srec_Ret errCode;	
};
struct Srec_State _srec;

// Handler for received data and address
static SrecHandlerDataCallback* __pSrecDataCallback = NULL;
static SrecHandlerAddrCallback* __pSrecAddrCallback = NULL;

void srec_init()
{
    _srec.lineState = SREC_LINE_STATE_INIT;
    _srec.debugChCount = 0;
    _srec.errCode = Srec_Ret_OK;
}

// Convert char to nybble
uint8_t chToNybble(int ch)
{
    if ((ch < '0') || ((ch > '9') && (ch < 'A')) || ((ch > 'F') && (ch < 'a')) || (ch > 'f')) {
        _srec.errCode = Srec_Ret_INVALID_NYBBLE;
#ifdef DEBUG_SREC_RX
        ee_printf("Nybble invalid %02x count %d\n", ch, _srec.debugChCount);
#endif
    }
    if (ch > '9')
        ch -= 7;
    return ch & 0xF;
}

// Handle a single char
Srec_Ret srec_handle_char(int ch)
{
    _srec.debugChCount++;
    // Handle based on state
    switch (_srec.lineState) {
    case SREC_LINE_STATE_INIT: {
        if (ch == 'S') {
            _srec.checksum = 0;
            _srec.lineState = SREC_LINE_STATE_RECTYPE;
            _srec.lastCharInvalid = 0;
        }
        else {
#ifdef DEBUG_SREC_RX
            if ((ch != '\n') && (ch != '\r'))
                if (!_srec.lastCharInvalid) {
                    ee_printf("I...%02x, count %d\n", ch, _srec.debugChCount);
                    _srec.lastCharInvalid = 1;
                }
#endif
            return Srec_Ret_IGNORED;
        }
        break;
    }
    case SREC_LINE_STATE_RECTYPE: {
        _srec.fieldCtr = 0;
        _srec.lineLen = 0;
        _srec.byteCtr = 0;
        // Get number of bytes in address - S1/S5/S9 == 2bytes, S2/S6/S8 == 3bytes, S3/S7 == 4 bytes
        _srec.addrBytes = (ch & 0x03) + 1;
        if ((ch & 0x0f) == 8)
            _srec.addrBytes = 3;
        else if ((ch & 0x0f) == 9)
            _srec.addrBytes = 2;
        // Handle record type
        switch (ch) {
        case '1':
        case '2':
        case '3': {
            _srec.recType = SREC_RECTYPE_DATA;
            _srec.lineState = SREC_LINE_STATE_LEN;
            break;
        }
        case '5':
        case '6': {
            _srec.recType = SREC_RECTYPE_COUNT;
            _srec.lineState = SREC_LINE_STATE_LEN;
            break;
        }
        case '7':
        case '8':
        case '9': {
            _srec.recType = SREC_RECTYPE_START;
            _srec.lineState = SREC_LINE_STATE_LEN;
            break;
        }
        default: {
#ifdef DEBUG_SREC_RX
            ee_printf("RECTYPE INVALID %d", ch);
#endif
            _srec.lineState = SREC_LINE_STATE_INIT;
            _srec.errCode = Srec_Ret_INVALID_RECTYPE;
            return Srec_Ret_INVALID_RECTYPE;
        }
        }
        break;
    }
    case SREC_LINE_STATE_LEN: {
        // Build length from nybbles
        _srec.lineLen = (_srec.lineLen << 4) + chToNybble(ch);
        _srec.fieldCtr++;
        // Check if done
        if (_srec.fieldCtr == 2) {
            // Now ready for address
            _srec.checksum += _srec.lineLen;
            _srec.fieldCtr = 0;
            _srec.addr = 0;
            _srec.byte = 0;
            _srec.lineState = SREC_LINE_STATE_ADDR;
        }
        break;
    }
    case SREC_LINE_STATE_ADDR: {
        // Build address from bytes
        _srec.byte = (_srec.byte << 4) + chToNybble(ch);
        _srec.fieldCtr++;
        // Address and Checksum
        if (_srec.fieldCtr % 2 == 0) {
            _srec.addr = (_srec.addr << 8) + _srec.byte;
            _srec.checksum += _srec.byte & 0xff;
            _srec.byte = 0;
        }
        // Done?
        if (_srec.fieldCtr == _srec.addrBytes * 2) {
            // Check if entry point record
            if (_srec.recType == SREC_RECTYPE_START) {
                // Set entry point
                _srec.entryAddr = _srec.addr;
                __pSrecAddrCallback(_srec.addr);
            }
            // Check if data record
            _srec.dataLen = 0;
            if (_srec.recType == SREC_RECTYPE_DATA)
                _srec.dataLen = _srec.lineLen - (_srec.fieldCtr / 2) - 1;
            // Check for data following
            if (_srec.dataLen > 0) {
                _srec.lineState = SREC_LINE_STATE_DATA;
            } else {
                // Ready for checksum
                _srec.lineState = SREC_LINE_STATE_CHECKSUM;
            }
            // New field starting
            _srec.byte = 0;
            _srec.byteIdx = 0;
            _srec.fieldCtr = 0;
        }
        break;
    }
    case SREC_LINE_STATE_DATA: {
        // Build address from bytes
        _srec.byte = (_srec.byte << 4) + chToNybble(ch);
        _srec.fieldCtr++;
        // Check if byte complete
        if (_srec.fieldCtr % 2 == 0) {
            // Checksum
            _srec.checksum += _srec.byte & 0xff;
            // Store to appropriate place
            if (_srec.byteIdx < MAX_SREC_DATA_LEN) {
                _srec.data[_srec.byteIdx] = _srec.byte;
            }
            // Next byte
            _srec.byteIdx++;
            _srec.byte = 0;
            // Check for end
            if (_srec.byteIdx >= _srec.dataLen) {
                // Check for checksum
                _srec.lineState = SREC_LINE_STATE_CHECKSUM;
                _srec.fieldCtr = 0;
                _srec.byte = 0;
            }
        }
        break;
    }
    case SREC_LINE_STATE_CHECKSUM: {
        // Build checksum from nybbles
        _srec.byte = (_srec.byte << 4) + chToNybble(ch);
        _srec.fieldCtr++;
        // Check if done
        if (_srec.fieldCtr == 2) {
            // Go back to initial state
            _srec.lineState = SREC_LINE_STATE_INIT;
            // Check if checksum correct
            if (_srec.byte != ((~_srec.checksum) & 0xff)) {
                return Srec_Ret_CHECKSUM_ERROR;
            } else {
                // Callback on new data
                if (_srec.recType == SREC_RECTYPE_DATA)
                    if (__pSrecDataCallback)
                        __pSrecDataCallback(_srec.addr, _srec.data, _srec.dataLen);
                return Srec_Ret_LINE_COMPLETE;
            }
        }
        break;
    }
    }
    return _srec.errCode;
}


void srec_decode(SrecHandlerDataCallback* pSrecDataCallback, SrecHandlerAddrCallback* pSrecAddrCallback,
			const uint8_t* pData, uint32_t len)
{
	__pSrecDataCallback = pSrecDataCallback;
	__pSrecAddrCallback = pSrecAddrCallback;

	// Init
	srec_init();

	// Send characters to parser
	for (uint32_t i = 0; i < len; i++)
	{
		srec_handle_char(pData[i]);
	}
}
