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
#include "nmalloc.h"

#define CMD_HANDLER_MAX_CMD_STR_LEN 200
#define MAX_INT_ARG_STR_LEN 10

static const char* FromCmdHandler = "CmdHander";

#define MAX_FILE_NAME_STR 50
char _receivedFileName[MAX_FILE_NAME_STR+1];
static uint8_t* _pReceivedFileDataPtr = NULL;
static int _receivedFileBufSize = 0;
static int _receivedFileBytesRx = 0;
static int _receivedBlockCount = 0;

// Structure for command handler state
void cmdHandler_sendChar(uint8_t ch)
{
    uart_send(ch);
}

void cmdHandler_sinkAddr(uint32_t addr)
{
    LogWrite(FromCmdHandler, LOG_DEBUG, "CmdHandler: got addr from SREC %04x", addr);
}

void cmdHandler_procCommand(const char* pCmdJson, const uint8_t* pData, int dataLen)
{
    LogWrite(FromCmdHandler, LOG_DEBUG, "cmdHandler_procCommand %s, dataLen %d", pCmdJson, dataLen);
    
    // Get the command string
    #define MAX_CMD_NAME_STR 30
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;

    // Check for simple commands
    if (strcmp(cmdName, "ufStart") == 0)
    {
        // Start a file upload - get file name
        if (!jsonGetValueForKey("fileName", pCmdJson, _receivedFileName, MAX_FILE_NAME_STR))
            return;

        // Get file length
        char fileLenStr[MAX_INT_ARG_STR_LEN+1];
        if (!jsonGetValueForKey("fileLen", pCmdJson, fileLenStr, MAX_INT_ARG_STR_LEN))
            return;
        int fileLen = strtol(fileLenStr, NULL, 10);
        if (fileLen <= 0)
            return;

        // Allocate space for file
        if (_pReceivedFileDataPtr != NULL)
            nmalloc_free((void**)(&_pReceivedFileDataPtr));
        _pReceivedFileDataPtr = nmalloc_malloc(fileLen);
        if (_pReceivedFileDataPtr)
        {
            _receivedFileBufSize = fileLen;
            _receivedFileBytesRx = 0;
            _receivedBlockCount = 0;

            // Debug
            LogWrite(FromCmdHandler, LOG_DEBUG, "efStart File %s, toPtr %08x, bufSize %d", 
                        _receivedFileName, _pReceivedFileDataPtr, _receivedFileBufSize);
        }
        else
        {
            LogWrite(FromCmdHandler, LOG_WARNING, "efStart unable to allocate memory for file %s, bufSize %d", 
                        _receivedFileName, _receivedFileBufSize);

        }
    }
    else if (strcmp(cmdName, "ufBlock") == 0)
    {
        // Check file reception in progress
        if (!_pReceivedFileDataPtr)
            return;

        // Get block location
        char blockStartStr[MAX_INT_ARG_STR_LEN+1];
        if (!jsonGetValueForKey("index", pCmdJson, blockStartStr, MAX_INT_ARG_STR_LEN))
            return;
        int blockStart = strtol(blockStartStr, NULL, 10);
        if (blockStart < 0)
            return;

        // Check if outside bounds of file length buffer
        if (blockStart + dataLen > _receivedFileBufSize)
            return;

        // Store the data
        memcpy(_pReceivedFileDataPtr+blockStart, pData, dataLen);
        _receivedFileBytesRx += dataLen;

        // Add to count of blocks
        _receivedBlockCount++;
        LogWrite(FromCmdHandler, LOG_DEBUG, "efBlock, len %d, blocks %d", 
                    _receivedFileBytesRx, _receivedBlockCount);
    }
    else if (strcmp(cmdName, "ufEnd") == 0)
    {
        // Check file reception in progress
        if (!_pReceivedFileDataPtr)
            return;

        // Check block count
        char blockCountStr[MAX_INT_ARG_STR_LEN+1];
        if (!jsonGetValueForKey("blockCount", pCmdJson, blockCountStr, MAX_INT_ARG_STR_LEN))
            return;
        int blockCount = strtol(blockCountStr, NULL, 10);
        if (blockCount != _receivedBlockCount)
        {
            LogWrite(FromCmdHandler, LOG_WARNING, "efEnd File %s, blockCount rx %d != sent %d", 
                    _receivedFileName, _receivedBlockCount, blockCount);
            return;
        }

        // Check type of file (assume extension is delimited by .)
        const char* pDot = strstr(_receivedFileName, ".");
        if (pDot != NULL)
        {
            if (stricmp(pDot, ".CMD") == 0)
            {
                LogWrite(FromCmdHandler, LOG_DEBUG, "efEnd CMD File %s, len %d", _receivedFileName, _receivedFileBytesRx);
                mc_generic_handle_file("{\"fileType\":\"cmd\"}", _pReceivedFileDataPtr, _receivedFileBytesRx);
                return;
            }
        }

        // Falling through to here so treat as binary file
        LogWrite(FromCmdHandler, LOG_DEBUG, "efEnd BIN File %s, len %d", _receivedFileName, _receivedFileBytesRx);
        mc_generic_handle_file("{\"fileType\":\"bin\"}", _pReceivedFileDataPtr, _receivedFileBytesRx);

    }
    else if (strcmp(cmdName, "cleartarget") == 0)
    {
        targetClear();
    }
    else if (strcmp(cmdName, "programtarget") == 0)
    {
        if (targetGetNumBlocks() == 0) {
            // Nothing new to write
            LogWrite(FromCmdHandler, LOG_DEBUG, "programtarget - nothing to write");
        } else {

            for (int i = 0; i < targetGetNumBlocks(); i++) {
                TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
                LogWrite(FromCmdHandler, LOG_DEBUG,"programtarget start %08x len %d", pBlock->start, pBlock->len);
                br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1, 0);
            }

            LogWrite(FromCmdHandler, LOG_DEBUG, "programtarget - written %d blocks", targetGetNumBlocks());
        }
    }
    else if (strcmp(cmdName, "resettarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "resettarget");
        br_reset_host();
    }
    else if (strcmp(cmdName, "ioclrtarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "ioclrtarget");
        // Fill IO "memory" with 0xff
        uint8_t tmpBuf[0x100];
        for (int kk = 0; kk < 0x100; kk++)
            tmpBuf[kk] = 0xff;
        br_write_block(0, tmpBuf, 0x100, 1, 1);         
    }
    else if (strcmp(cmdName, "filetarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "filetarget, len %d", dataLen);
        mc_generic_handle_file(pCmdJson, pData, dataLen);
    }
    else if (strcmp(cmdName, "srectarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "srectarget, len %d", dataLen);
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

    // LogWrite(FromCmdHandler, LOG_DEBUG, "%s, cmdLen %d byte0 %02x, datalen %d", cmdStr, cmdStrLen, pDataPtr[0], dataLen);
    cmdHandler_procCommand(cmdStr, pDataPtr, dataLen);
}

// Init the destinations for SREC and TREC records
void cmdHandler_init()
{
    minihdlc_init(cmdHandler_sendChar, cmdHandler_frameHandler);
    _receivedFileName[0] = 0;
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
