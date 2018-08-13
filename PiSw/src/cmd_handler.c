// Bus Raider
// Rob Dobson 2018

#include "cmd_handler.h"
#include "bare_metal_pi_zero.h"
#include "ee_printf.h"
#include "uart.h"
#include "utils.h"
#include "target_memory_map.h"
#include "busraider.h"
#include "minihdlc.h"
#include "ee_printf.h"
#include "srecparser.h"

#define CMD_HANDLER_MAX_CMD_STR_LEN 200

// Structure for command handler state
void cmdHandler_sendChar(uint8_t ch)
{
    uart_send(ch);
}

void cmdHandler_sinkAddr(uint32_t addr)
{
    ee_printf("CmdHandler: got addr from SREC %04x\n", addr);
}

void cmdHandler_procCommand(const char* pCmdStr, const uint8_t* pData, int dataLen)
{
    // Check for simple commands
    if (strcmp(pCmdStr, "srectarget") == 0)
    {
        ee_printf("CmdHandler: RxCmd srectarget, byte0 %02x len %d\n", pData[0], dataLen);
        targetClear();
        srec_decode(targetDataBlockCallback, cmdHandler_sinkAddr, pData, dataLen);
    }
    else if (strcmp(pCmdStr, "programtarget") == 0)
    {
        if (targetGetNumBlocks() == 0) {
            // Nothing new to write
            ee_printf("CmdHandler: programtarget - nothing to write\n");
        } else {

            for (int i = 0; i < targetGetNumBlocks(); i++) {
                TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
                ee_printf("CmdHandler: programtarget start %08x len %d\n", pBlock->start, pBlock->len);
                br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1, 0);
            }

            ee_printf("CmdHandler: programtarget - written %d blocks\n", targetGetNumBlocks());
        }
    }
    else if (strcmp(pCmdStr, "resettarget") == 0)
    {
        ee_printf("CmdHandler: RxCmd resettarget\n");
        br_reset_host();
    }
    else if (strcmp(pCmdStr, "ioclrtarget") == 0)
    {
        ee_printf("CmdHandler: RxCmd ioclrtarget\n");
        // Fill IO "memory" with 0xff
        uint8_t tmpBuf[0x100];
        for (int kk = 0; kk < 0x100; kk++)
            tmpBuf[kk] = 0xff;
        br_write_block(0, tmpBuf, 0x100, 1, 1);         
    }
}

void cmdHandler_frameHandler(const uint8_t *framebuffer, int framelength)
{
    ee_printf("HDLC frame received, len %d\n", framelength);

    // Extract command string
    char cmdStr[CMD_HANDLER_MAX_CMD_STR_LEN+1];
    int cmdStrLen = 0;
    for (int i = 0; i < framelength; i++)
    {
        if (framebuffer[i] == 0)
            break;
        if (i < CMD_HANDLER_MAX_CMD_STR_LEN)
        {
            cmdStr[cmdStrLen++] = framebuffer[i];
        }
    }
    cmdStr[cmdStrLen] = 0;

    // Process command
    const uint8_t* pDataPtr = framebuffer + cmdStrLen + 1;
    int dataLen = framelength - cmdStrLen - 1;
    if (dataLen < 0)
        dataLen = 0;

    ee_printf("Command str %s, cmdLen %d byte0 %02x, datalen %d\n", cmdStr, cmdStrLen, pDataPtr[0], dataLen);
    cmdHandler_procCommand(cmdStr, pDataPtr, dataLen);
}

// Init the destinations for SREC and TREC records
void cmdHandler_init()
{
    minihdlc_init(cmdHandler_sendChar, cmdHandler_frameHandler);
}

void cmdHandler_service()
{
    // Handle serial communication
    for (int rxCtr = 0; rxCtr < 100; rxCtr++) {
        if (!uart_poll())
            break;

        // Show char received
        int ch = uart_read_byte();
        // ee_printf("%02x ", ch);

        // Handle char
        minihdlc_char_receiver(ch);
    }
}
