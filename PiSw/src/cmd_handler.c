// Bus Raider
// Rob Dobson 2018

#include "cmd_handler.h"
#include "bare_metal_pi_zero.h"
#include "ee_printf.h"
#include "uart.h"
#include "utils.h"
#include "target_memory_map.h"
#include "busraider.h"

#define MAX_SREC_DATA_LEN 200

// States while decoding a line of Motorola SREC format
typedef enum {
    CMDHANDLER_STATE_INIT,
    CMDHANDLER_STATE_RECTYPE,
    CMDHANDLER_STATE_LEN,
    CMDHANDLER_STATE_ADDR,
    CMDHANDLER_STATE_DATA,
    CMDHANDLER_STATE_CHECKSUM
} CmdHandler_State;

// Type of Motorola SREC record
typedef enum {
    CMDHANDLER_RECTYPE_DATA,
    CMDHANDLER_RECTYPE_START,
    CMDHANDLER_RECTYPE_COUNT
} CmdHandler_RecType;

// Destination for data
// - records prefixed with S (i.e. regular SRECs) are considered bootloader records
// - records prefixed with T are considered target program/data
typedef enum {
    CMDHANDLER_DEST_BOOTLOAD,
    CMDHANDLER_DEST_TARGET
} CmdHandler_Dest;

// State vars
CmdHandler_State __cmdHandler_state = 0;
CmdHandler_Dest __cmdHandler_dest = CMDHANDLER_DEST_TARGET;
CmdHandler_RecType __cmdHandler_recType = CMDHANDLER_RECTYPE_DATA;
uint32_t __cmdHandler_checksum = 0;
uint32_t __cmdHandler_entryAddr = 0;
int __cmdHandler_addrBytes = 0;
int __cmdHandler_fieldCtr = 0;
int __cmdHandler_byteCtr = 0;
int __cmdHandler_len = 0;
int __cmdHandler_dataLen = 0;
int __cmdHandler_byteIdx = 0;
uint32_t __cmdHandler_addr = 0;
uint8_t __cmdHandler_byte = 0;
uint8_t __cmdHandler_data[MAX_SREC_DATA_LEN];
int __cmdHandler_lastCharInvalid = 0;
int __cmdHandler_debugChCount = 0;
int __cmdHandler_errCode = 0;

// Handler for received data
TCmdHandlerDataBlockCallback* __cmdHandler_pDataBlockCallback;

// Init the destinations for SREC and TREC records
void cmdHandler_init(TCmdHandlerDataBlockCallback* pDataBlockCallback)
{
    __cmdHandler_pDataBlockCallback = pDataBlockCallback;
    __cmdHandler_state = CMDHANDLER_STATE_INIT;
    __cmdHandler_debugChCount = 0;
    __cmdHandler_errCode = CMDHANDLER_RET_OK;
}

void cmdHandler_clearError()
{
    __cmdHandler_errCode = CMDHANDLER_RET_OK;
    __cmdHandler_debugChCount = 0;
}

int cmdHandler_getError()
{
    return __cmdHandler_errCode;
}

// Convert char to nybble
uint8_t chToNybble(int ch)
{
    if ((ch < '0') || ((ch > '9') && (ch < 'A')) || ((ch > 'F') && (ch < 'a')) || (ch > 'f')) {
        __cmdHandler_errCode = CMDHANDLER_RET_INVALID_NYBBLE;
#ifdef DEBUG_SREC_RX
        ee_printf("Nybble invalid %02x count %d\n", ch, __cmdHandler_debugChCount);
#endif
    }
    if (ch > '9')
        ch -= 7;
    return ch & 0xF;
}

// Handle a single char
CmdHandler_Ret cmdHandler_handle_char(int ch)
{
    __cmdHandler_debugChCount++;
    // Handle based on state
    switch (__cmdHandler_state) {
    case CMDHANDLER_STATE_INIT: {
        if ((ch == 'S') || (ch == 'T')) {
            __cmdHandler_checksum = 0;
            __cmdHandler_state = CMDHANDLER_STATE_RECTYPE;
            __cmdHandler_dest = (ch == 'S') ? CMDHANDLER_DEST_BOOTLOAD : CMDHANDLER_DEST_TARGET;
            __cmdHandler_lastCharInvalid = 0;
        }
        // else if ((ch == 'g') || (ch == 'G'))
        // {
        // 	// Go to start address
        // 	__cmdHandler_lastCharInvalid = 0;
        // 	utils_goto(__cmdHandler_entryAddr);
        // }
        else {
#ifdef DEBUG_SREC_RX
            if ((ch != '\n') && (ch != '\r'))
                if (!__cmdHandler_lastCharInvalid) {
                    ee_printf("I...%02x, count %d\n", ch, __cmdHandler_debugChCount);
                    __cmdHandler_lastCharInvalid = 1;
                }
#endif
            return CMDHANDLER_RET_IGNORED;
        }
        break;
    }
    case CMDHANDLER_STATE_RECTYPE: {
        __cmdHandler_fieldCtr = 0;
        __cmdHandler_len = 0;
        __cmdHandler_byteCtr = 0;
        // Get number of bytes in address - S1/S5/S9 == 2bytes, S2/S6/S8 == 3bytes, S3/S7 == 4 bytes
        __cmdHandler_addrBytes = (ch & 0x03) + 1;
        if ((ch & 0x0f) == 8)
            __cmdHandler_addrBytes = 3;
        else if ((ch & 0x0f) == 9)
            __cmdHandler_addrBytes = 2;
        // Handle record type
        switch (ch) {
        case '1':
        case '2':
        case '3': {
            __cmdHandler_recType = CMDHANDLER_RECTYPE_DATA;
            __cmdHandler_state = CMDHANDLER_STATE_LEN;
            break;
        }
        case '5':
        case '6': {
            __cmdHandler_recType = CMDHANDLER_RECTYPE_COUNT;
            __cmdHandler_state = CMDHANDLER_STATE_LEN;
            break;
        }
        case '7':
        case '8':
        case '9': {
            __cmdHandler_recType = CMDHANDLER_RECTYPE_START;
            __cmdHandler_state = CMDHANDLER_STATE_LEN;
            break;
        }
        default: {
#ifdef DEBUG_SREC_RX
            ee_printf("RECTYPE INVALID %d", ch);
#endif
            __cmdHandler_state = CMDHANDLER_STATE_INIT;
            __cmdHandler_errCode = CMDHANDLER_RET_INVALID_RECTYPE;
            return CMDHANDLER_RET_INVALID_RECTYPE;
        }
        }
        break;
    }
    case CMDHANDLER_STATE_LEN: {
        // Build length from nybbles
        __cmdHandler_len = (__cmdHandler_len << 4) + chToNybble(ch);
        __cmdHandler_fieldCtr++;
        // Check if done
        if (__cmdHandler_fieldCtr == 2) {
            // Now ready for address
            __cmdHandler_checksum += __cmdHandler_len;
            __cmdHandler_fieldCtr = 0;
            __cmdHandler_addr = 0;
            __cmdHandler_byte = 0;
            __cmdHandler_state = CMDHANDLER_STATE_ADDR;
        }
        break;
    }
    case CMDHANDLER_STATE_ADDR: {
        // Build address from bytes
        __cmdHandler_byte = (__cmdHandler_byte << 4) + chToNybble(ch);
        __cmdHandler_fieldCtr++;
        // Address and Checksum
        if (__cmdHandler_fieldCtr % 2 == 0) {
            __cmdHandler_addr = (__cmdHandler_addr << 8) + __cmdHandler_byte;
            __cmdHandler_checksum += __cmdHandler_byte & 0xff;
            __cmdHandler_byte = 0;
        }
        // Done?
        if (__cmdHandler_fieldCtr == __cmdHandler_addrBytes * 2) {
            // Check if entry point record
            if (__cmdHandler_recType == CMDHANDLER_RECTYPE_START) {
                // Set entry point
                __cmdHandler_entryAddr = __cmdHandler_addr;
            }
            // Check if data record
            __cmdHandler_dataLen = 0;
            if (__cmdHandler_recType == CMDHANDLER_RECTYPE_DATA)
                __cmdHandler_dataLen = __cmdHandler_len - (__cmdHandler_fieldCtr / 2) - 1;
            // Check for data following
            if (__cmdHandler_dataLen > 0) {
                __cmdHandler_state = CMDHANDLER_STATE_DATA;
            } else {
                // Ready for checksum
                __cmdHandler_state = CMDHANDLER_STATE_CHECKSUM;
            }
            // New field starting
            __cmdHandler_byte = 0;
            __cmdHandler_byteIdx = 0;
            __cmdHandler_fieldCtr = 0;
        }
        break;
    }
    case CMDHANDLER_STATE_DATA: {
        // Build address from bytes
        __cmdHandler_byte = (__cmdHandler_byte << 4) + chToNybble(ch);
        __cmdHandler_fieldCtr++;
        // Check if byte complete
        if (__cmdHandler_fieldCtr % 2 == 0) {
            // Checksum
            __cmdHandler_checksum += __cmdHandler_byte & 0xff;
            // Store to appropriate place
            if (__cmdHandler_byteIdx < MAX_SREC_DATA_LEN) {
                __cmdHandler_data[__cmdHandler_byteIdx] = __cmdHandler_byte;
            }
            // if (__cmdHandler_dest == CMDHANDLER_DEST_BOOTLOAD)
            // {
            // 	if (__cmdHandler_addr + __cmdHandler_byteIdx < __cmdHandler_srec_maxlen)
            // 	{
            // 		__pCmdHandler_srec_base[__cmdHandler_addr + __cmdHandler_byteIdx] = __cmdHandler_byte;
            // 	}
            // }
            // else
            // {
            // 	if (__cmdHandler_addr + __cmdHandler_byteIdx < __cmdHandler_trec_maxlen)
            // 	{
            // 		__pCmdHandler_trec_base[__cmdHandler_addr + __cmdHandler_byteIdx] = __cmdHandler_byte;
            // 	}

            // }
            // Next byte
            __cmdHandler_byteIdx++;
            __cmdHandler_byte = 0;
            // Check for end
            if (__cmdHandler_byteIdx >= __cmdHandler_dataLen) {
                // Check for checksum
                __cmdHandler_state = CMDHANDLER_STATE_CHECKSUM;
                __cmdHandler_fieldCtr = 0;
                __cmdHandler_byte = 0;
            }
        }
        break;
    }
    case CMDHANDLER_STATE_CHECKSUM: {
        // Build checksum from nybbles
        __cmdHandler_byte = (__cmdHandler_byte << 4) + chToNybble(ch);
        __cmdHandler_fieldCtr++;
        // Check if done
        if (__cmdHandler_fieldCtr == 2) {
            // Go back to initial state
            __cmdHandler_state = CMDHANDLER_STATE_INIT;
            // Check if checksum correct
            if (__cmdHandler_byte != ((~__cmdHandler_checksum) & 0xff)) {
                return CMDHANDLER_RET_CHECKSUM_ERROR;
            } else {
                // Callback on new data
                if (__cmdHandler_recType == CMDHANDLER_RECTYPE_DATA)
                    if (__cmdHandler_pDataBlockCallback)
                        __cmdHandler_pDataBlockCallback(__cmdHandler_addr, __cmdHandler_data, __cmdHandler_dataLen, __cmdHandler_dest);
                return CMDHANDLER_RET_LINE_COMPLETE;
            }
        }
        break;
    }
    }
    return __cmdHandler_errCode;
}

void cmdHandler_service()
{
    // Handle serial communication
    for (int rxCtr = 0; rxCtr < 100; rxCtr++) {
        if (!uart_poll())
            break;

        // Show char received
        int ch = uart_read_byte();
        // ee_printf("%c", ch);

        // Offer to the cmd_handler
        CmdHandler_Ret retc = cmdHandler_handle_char(ch);
        if (retc == CMDHANDLER_RET_CHECKSUM_ERROR) {
            ee_printf("Checksum error %d\n", retc);
            // usleep(3000000);
        } else if ((retc == CMDHANDLER_RET_INVALID_RECTYPE) || (retc == CMDHANDLER_RET_INVALID_NYBBLE)) {
            ee_printf("Error receiving from serial %d\n", retc);
            // // Discard remaining chars
            // usleep(100000);
            // while(uart_poll())
            // {
            //     usleep(1000);
            //     while(uart_poll())
            //         uart_read_byte();
            // }
        }

        // Check handled
        if (retc == CMDHANDLER_RET_IGNORED) {
            // if (ch == 'x')
            // {
            //     // Test
            //     unsigned int testBaseAddr = 0;
            //     unsigned int testLen = 10000;
            //     targetClear();
            //     BR_RETURN_TYPE brRetc = br_read_block(testBaseAddr, __pTargetBuffer, testLen, 1, 0);
            //     gfx_term_putstring("ReadBlock=");
            //     gfx_term_putchar(brRetc + '0');
            //     gfx_term_putstring("\n");
            //     for (int i = 0; i < 0x10; i++)
            //     {
            //         ee_printf("%02x ", __pTargetBuffer[i]);
            //     }
            //     ee_printf("\n");

            //     // Test data
            //     for (unsigned int i = 0; i < testLen; i++)
            //     {
            //         __pTargetBuffer[i] = (i * 23487) / 3;
            //     }

            //     brRetc = br_write_block(testBaseAddr, __pTargetBuffer, testLen, 1, 0);

            //     unsigned char pTestBuffer[testLen];
            //     brRetc = br_read_block(testBaseAddr, pTestBuffer, testLen, 1, 0);
            //     gfx_term_putstring("ReadBlock=");
            //     gfx_term_putchar(brRetc + '0');
            //     gfx_term_putstring("\n");
            //     int errFound = 0;
            //     unsigned int errAddr = 0;
            //     for (unsigned int i = 0; i < testLen; i++)
            //     {
            //         if (pTestBuffer[i] != __pTargetBuffer[i])
            //         {
            //             errFound = 1;
            //             errAddr = i;
            //             break;
            //         }

            //     }
            //     if (errFound)
            //     {
            //         ee_printf("Error at %08x\n", errAddr);
            //         for (int i = 0; i < 5; i++)
            //         {
            //             ee_printf("%02x %02x\n", pTestBuffer[(errAddr+i) % testLen], __pTargetBuffer[(errAddr+i) % testLen]);
            //         }
            //     }

            // }
            if (ch == 'g') {
                // for (int k = 0; k < 10; k++)
                // {
                //     for (int i = 0; i < 0x10; i++)
                //     {
                //         ee_printf("%02x ", __pTargetBuffer[k*0x10+i]);
                //     }
                //     ee_printf("\n");
                // }

                if (targetGetNumBlocks() == 0) {
                    // Nothing new to write
                    ee_printf("Nothing new to write to target\n");
                } else {

                    for (int i = 0; i < targetGetNumBlocks(); i++) {
                        TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
                        ee_printf("%08x %08x\n", pBlock->start, pBlock->len);


                        // TEST
                        uint8_t tmpBuf[0x100];
                        for (int kk = 0; kk < 0x100; kk++)
                            tmpBuf[kk] = 0xff;
                        br_write_block(0, tmpBuf, 0x100, 1, 1);                            


                        br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1, 0);
                    }

                    ee_printf("Written %d blocks, now resetting host ...\n", targetGetNumBlocks());
                    br_reset_host();

                    targetClear();
                }
            }
            //     else if (ch == 'z')
            //     {
            //         // unsigned char pTestBuffer[0x400];
            //         // br_read_block(0x3c00, pTestBuffer, 0x400, 1);
            //         // for (int k = 0; k < 16; k++)
            //         // {
            //         //     for (int i = 0; i < 64; i++)
            //         //     {
            //         //         ee_printf("%c", pTestBuffer[k*64+i]);
            //         //     }
            //         //     ee_printf("\n");
            //         // }
            //         br_write_block(0x0, __pTargetBuffer, 3, 1, 0);
            //         br_write_block(0x8000, __pTargetBuffer+0x8000, 0x4000, 1, 0);

            //         int testLen = 20;
            //         unsigned char pTestBuffer[testLen];
            //         int brRetc = br_read_block(0xa4a0, pTestBuffer, testLen, 1, 0);
            //         gfx_term_putstring("ReadBlock=");
            //         gfx_term_putchar(brRetc + '0');
            //         gfx_term_putstring("\n");
            //         for ( int i = 0; i < testLen; i++)
            //         {
            //             ee_printf("%02x ", pTestBuffer[i]);
            //             if (pTestBuffer[i] != __pTargetBuffer[0xa4a0+i])
            //                 ee_printf("(%02x)", __pTargetBuffer[0xa4a0+i]);
            //         }
            //         ee_printf("\n");

            //         br_reset_host();

            //         targetClear();
            //     }
            //     else if (ch == 'q')
            //     {
            //         unsigned char pTestBuffer[0x400];
            //         br_read_block(0x3c00, pTestBuffer, 0x400, 1, 0);
            //         for (int k = 0; k < 16; k++)
            //         {
            //             for (int i = 0; i < 64; i++)
            //             {
            //                 ee_printf("%c", pTestBuffer[k*64+i]);
            //             }
            //             ee_printf("\n");
            //         }
            //     }
            //     else if (ch == 'm')
            //     {
            //         ee_printf("Blocks %d\n", __targetMemoryBlockLastIdx);
            //         for (int i = 0; i < __targetMemoryBlockLastIdx; i++)
            //         {
            //             ee_printf("%08x %08x\n", __targetMemoryBlocks[i].start, __targetMemoryBlocks[i].len);
            //         }
            //     }
        }
    }
}
