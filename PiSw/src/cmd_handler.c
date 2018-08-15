// Bus Raider
// Rob Dobson 2018

#include "cmd_handler.h"
#include "bare_metal_pi_zero.h"
#include "ee_printf.h"
#include "uart.h"
#include "utils.h"
#include "rdutils.h"
#include "mc_generic.h"
#include "target_memory_map.h"
#include "busraider.h"
#include "minihdlc.h"
#include "ee_printf.h"
#include "srecparser.h"

#define CMD_HANDLER_MAX_CMD_STR_LEN 200

static const char* FromCmdHandler = "CmdHander";

// Structure for command handler state
void cmdHandler_sendChar(uint8_t ch)
{
    uart_send(ch);
}

void cmdHandler_sinkAddr(uint32_t addr)
{
    LogWrite(FromCmdHandler, LOG_DEBUG, "CmdHandler: got addr from SREC %04x\n", addr);
}

void cmdHandler_procCommand(const char* pCmdJson, const uint8_t* pData, int dataLen)
{
    // ee_printf("cmdHandler_procCommand %s\n", pCmdJson);
    
    // Get the command string
    #define MAX_CMD_NAME_STR 100
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;

    // Check for simple commands
    if (strcmp(cmdName, "cleartarget") == 0)
    {
        targetClear();
    }
    else if (strcmp(cmdName, "programtarget") == 0)
    {
        if (targetGetNumBlocks() == 0) {
            // Nothing new to write
            LogWrite(FromCmdHandler, LOG_DEBUG, "programtarget - nothing to write\n");
        } else {

            for (int i = 0; i < targetGetNumBlocks(); i++) {
                TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
                LogWrite(FromCmdHandler, LOG_DEBUG,"programtarget start %08x len %d\n", pBlock->start, pBlock->len);
                br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1, 0);
            }

            LogWrite(FromCmdHandler, LOG_DEBUG, "programtarget - written %d blocks\n", targetGetNumBlocks());
        }
    }
    else if (strcmp(cmdName, "resettarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "resettarget\n");
        br_reset_host();
    }
    else if (strcmp(cmdName, "ioclrtarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "ioclrtarget\n");
        // Fill IO "memory" with 0xff
        uint8_t tmpBuf[0x100];
        for (int kk = 0; kk < 0x100; kk++)
            tmpBuf[kk] = 0xff;
        br_write_block(0, tmpBuf, 0x100, 1, 1);         
    }
    else if (strcmp(cmdName, "filetarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "filetarget, len %d\n", dataLen);
        mc_generic_handle_file(pCmdJson, pData, dataLen);
    }
    else if (strcmp(cmdName, "srectarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "srectarget, len %d\n", dataLen);
        srec_decode(targetDataBlockStore, cmdHandler_sinkAddr, pData, dataLen);
    }
}

void cmdHandler_frameHandler(const uint8_t *framebuffer, int framelength)
{
    // ee_printf("HDLC frame received, len %d\n", framelength);

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

    // LogWrite(FromCmdHandler, LOG_DEBUG, "%s, cmdLen %d byte0 %02x, datalen %d\n", cmdStr, cmdStrLen, pDataPtr[0], dataLen);
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
