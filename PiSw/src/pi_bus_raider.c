// Bus Raider
// Rob Dobson 2018

#include "globaldefs.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
#include "wgfx.h"
#include "nmalloc.h"
#include "ee_printf.h"
#include "piwiring.h"
#include "../uspi\include\uspi\types.h"
#include "../uspi/include/uspi.h"
#include "busraider.h"
#include "cmd_handler.h"
#include "mc_generic.h"
#include "target_memory_map.h"
#include "rdutils.h"

// Baud rate
#define MAIN_UART_BAUD_RATE 921600

// Heap space
extern unsigned int pheap_space;
extern unsigned int heap_sz;

static void _keypress_raw_handler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // ee_printf("KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
    mc_generic_handle_key(ucModifiers, rawKeys);
}

unsigned long __lastDisplayUpdateUs = 0;

void main_loop()
{
    ee_printf("Waiting for UART data (%d,8,N,1)\n", MAIN_UART_BAUD_RATE);

    McGenericDescriptor* pMcDescr = mc_generic_get_descriptor();
    const unsigned long reqUpdateUs = 1000000 / pMcDescr->displayRefreshRatePerSec;

    while (1) {
        // Handle target machine display updates
        if (timer_isTimeout(micros(), __lastDisplayUpdateUs, reqUpdateUs)) {
            // Check valid
            mc_generic_handle_disp();
            __lastDisplayUpdateUs = micros();
        }

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

        timer_poll();
    }
}

void entry_point()
{
    // Heap init
    nmalloc_set_memory_area((unsigned char*)(pheap_space), heap_sz);

    // UART
    uart_init(MAIN_UART_BAUD_RATE);

    // Target machine memory and command handler
    targetClear();
    cmdHandler_init(targetDataBlockCallback);

    // Set to TRS80 Model1
    mc_generic_set("TRS80Model1");
    McGenericDescriptor* pMcDescr = mc_generic_get_descriptor();

    // Graphics system
    wgfx_init(1366, 768);

    // Allocate display space
    int windowBorderWidth = 5;
    wgfx_set_window(0, -1, 0, pMcDescr->displayPixelsX, pMcDescr->displayPixelsY,
        pMcDescr->displayCellX, pMcDescr->displayCellY, 2, 1,
        pMcDescr->pFont, pMcDescr->displayForeground, pMcDescr->displayBackground,
        windowBorderWidth, 8);
    wgfx_set_window(1, 0, pMcDescr->displayPixelsY + windowBorderWidth*2 + 10, -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        windowBorderWidth, 8);
    wgfx_set_console_window(1);

    // Initial message
    wgfx_set_fg(11);
    ee_printf("RC2014 Bus Raider V1.0\n");
    ee_printf("Rob Dobson 2018 (inspired by PiGFX)\n\n");
    wgfx_set_fg(15);

    // Init timers
    timers_init();

    // USB
    if (USPiInitialize()) {
        ee_printf("Checking for keyboards...");

        if (USPiKeyboardAvailable()) {
            USPiKeyboardRegisterKeyStatusHandlerRaw(_keypress_raw_handler);
            ee_printf("keyboard found\n");
        } else {
            wgfx_set_fg(9);
            ee_printf("keyboard not found\n");
            wgfx_set_fg(15);
        }
    } else {
        ee_printf("USB initialization failed\n");
    }
    ee_printf("\n");

    // Debug show colour palette
    // for (int i = 0; i < 255; i++)
    // {
    //     wgfx_set_fg(i);
    //     ee_printf("%02d ", i);
    // }
    // wgfx_set_fg(15);

    // Initialise the interrupt handler
    uart_init_irq();

    // Bus raider setup
    br_init();

    // Start the main loop
    main_loop();
}
