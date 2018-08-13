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

                // TEST for IO
                uint8_t tmpBuf[0x100];
                for (int kk = 0; kk < 0x100; kk++)
                    tmpBuf[kk] = 0xff;
                br_write_block(0, tmpBuf, 0x100, 1, 1);                            


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


        // Check handled
        // if (retc == Srec_Ret_IGNORED) {
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












            // if (ch == 'g') {
            //     // for (int k = 0; k < 10; k++)
            //     // {
            //     //     for (int i = 0; i < 0x10; i++)
            //     //     {
            //     //         ee_printf("%02x ", __pTargetBuffer[k*0x10+i]);
            //     //     }
            //     //     ee_printf("\n");
            //     // }

            //     if (targetGetNumBlocks() == 0) {
            //         // Nothing new to write
            //         ee_printf("Nothing new to write to target\n");
            //     } else {

            //         for (int i = 0; i < targetGetNumBlocks(); i++) {
            //             TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
            //             ee_printf("%08x %08x\n", pBlock->start, pBlock->len);


            //             // TEST
            //             uint8_t tmpBuf[0x100];
            //             for (int kk = 0; kk < 0x100; kk++)
            //                 tmpBuf[kk] = 0xff;
            //             br_write_block(0, tmpBuf, 0x100, 1, 1);                            


            //             br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1, 0);
            //         }

            //         ee_printf("Written %d blocks, now resetting host ...\n", targetGetNumBlocks());
            //         br_reset_host();

            //         targetClear();
            //     }
            // }













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
//         // }
//     }
// }
