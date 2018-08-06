// Bus Raider V1.3

#include "globaldefs.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
#include "framebuffer.h"
#include "wgfx.h"
#include "nmalloc.h"
#include "ee_printf.h"
#include "piwiring.h"
#include "../uspi\include\uspi\types.h"
#include "../uspi/include/uspi.h"
#include "busraider.h"
#include "cmd_handler.h"
#include "mc_generic.h"
#include "rdutils.h"
#include "target_memory_map.h"

// Heap space
extern unsigned int pheap_space;
extern unsigned int heap_sz;

static void _keypress_raw_handler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // ee_printf("KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
    mc_generic_handle_key(ucModifiers, rawKeys);
}

void initialize_framebuffer(WgfxFont* pSystemFont)
{
    usleep(10000);
    fb_release();

    unsigned char* p_fb=0;
    unsigned int fbsize;
    unsigned int pitch;

    unsigned int p_w = 1366;
    unsigned int p_h = 768;
    unsigned int v_w = p_w;
    unsigned int v_h = p_h;

    fb_init(p_w, p_h, 
            v_w, v_h,
            8, 
            (void*)&p_fb, 
            &fbsize, 
            &pitch);

    fb_set_xterm_palette();

    if (fb_get_phisical_buffer_size(&p_w, &p_h) != FB_SUCCESS)
    {
        //cout("fb_get_phisical_buffer_size error");cout_endl();
    }
    //cout("phisical fb size: "); cout_d(p_w); cout("x"); cout_d(p_h); cout_endl();

    usleep(10000);
    wgfx_init(p_fb, v_w, v_h, pitch, fbsize, pSystemFont); 
    wgfx_clear();
}

unsigned long __lastDisplayUpdateUs = 0;

void main_loop()
{
    ee_printf("Waiting for UART data (921600,8,N,1)\n");

    McGenericDescriptor* pMcDescr = mc_generic_get_descriptor();
    const unsigned long reqUpdateUs = 1000000 / pMcDescr->displayRefreshRatePerSec;

    while(1)
    {
        // Handle target machine display updates
        if (rdutils_isTimeout(time_microsec(), __lastDisplayUpdateUs, reqUpdateUs))
        {
            // Check valid 
            mc_generic_handle_disp();
            __lastDisplayUpdateUs = time_microsec();
        }


        // Handle serial communication
        for (int rxCtr = 0; rxCtr < 100; rxCtr++)
        {
            if (!uart_poll())
                break;

            // Show char received
            int ch = uart_read_byte();
            // ee_printf("%c", ch);
            
            // Offer to the cmd_handler
            CmdHandler_Ret retc = cmdHandler_handle_char(ch);
            if (retc == CMDHANDLER_RET_CHECKSUM_ERROR)
            {
                ee_printf("Checksum error %d\n", retc);
                // usleep(3000000);
            }
            else if ((retc == CMDHANDLER_RET_INVALID_RECTYPE) || (retc == CMDHANDLER_RET_INVALID_NYBBLE))
            {
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
            if (retc == CMDHANDLER_RET_IGNORED)
            {
                // if (ch == 'x')
                // {
                //     // Test
                //     unsigned int testBaseAddr = 0;
                //     unsigned int testLen = 10000;
                //     targetClear();
                //     BR_RETURN_TYPE brRetc = br_read_block(testBaseAddr, __pTargetBuffer, testLen, 1);
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

                //     brRetc = br_write_block(testBaseAddr, __pTargetBuffer, testLen, 1);

                //     unsigned char pTestBuffer[testLen];
                //     brRetc = br_read_block(testBaseAddr, pTestBuffer, testLen, 1);
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
                if (ch == 'g')
                {
                    // for (int k = 0; k < 10; k++)
                    // {
                    //     for (int i = 0; i < 0x10; i++)
                    //     {
                    //         ee_printf("%02x ", __pTargetBuffer[k*0x10+i]);
                    //     }
                    //     ee_printf("\n");
                    // }

                    if (targetGetNumBlocks() == 0)
                    {
                        // Nothing new to write
                        ee_printf("Nothing new to write to target\n");
                    }
                    else
                    {

                        for (int i = 0; i < targetGetNumBlocks(); i++)
                        {
                            TargetMemoryBlock* pBlock = targetGetMemoryBlock(i);
                            // ee_printf("%08x %08x\n", __targetMemoryBlocks[i].start, __targetMemoryBlocks[i].len);
                            br_write_block(pBlock->start, targetMemoryPtr() + pBlock->start, pBlock->len, 1);
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
            //         br_write_block(0x0, __pTargetBuffer, 3, 1);
            //         br_write_block(0x8000, __pTargetBuffer+0x8000, 0x4000, 1);

            //         int testLen = 20;
            //         unsigned char pTestBuffer[testLen];
            //         int brRetc = br_read_block(0xa4a0, pTestBuffer, testLen, 1);
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
            //         br_read_block(0x3c00, pTestBuffer, 0x400, 1);
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
    uart_init();

    // Target machine memory and command handler
    targetClear();
    cmdHandler_init(targetDataBlockCallback);

    // Set to TRS80 Model1
    mc_generic_set("TRS80Model1");
    McGenericDescriptor* pMcDescr = mc_generic_get_descriptor();

    // Frame buffer
    initialize_framebuffer(pMcDescr->pFont);

    // Allocate display space
    wgfx_set_window(0, 0, 0, pMcDescr->displayPixelsX, pMcDescr->displayPixelsY,
                pMcDescr->displayCellX, pMcDescr->displayCellY, 2, 1, pMcDescr->pFont);
    wgfx_set_window(1, 0, pMcDescr->displayPixelsY, -1, -1, 8, 24, 2, 1, pMcDescr->pFont);
    wgfx_set_console_window(1);

    // Initial message
    ee_printf("RC2014 Bus Raider V1.0\n");
    ee_printf("Rob Dobson (inspired by PiGFX)\n\n");

    // Init timers
    timers_init();

    // USB
    // ee_printf("Initializing USB\n");
    if(USPiInitialize())
    {
        ee_printf("Initialization OK!\n");
        ee_printf("Checking for keyboards...\n");

        if (USPiKeyboardAvailable())
        {
            USPiKeyboardRegisterKeyStatusHandlerRaw(_keypress_raw_handler);
            wgfx_set_fg(10);
            ee_printf("Keyboard found.\n");
            wgfx_set_fg(15);
        }
        else
        {
            wgfx_set_fg(9);
            ee_printf("No keyboard found.\n");
            wgfx_set_fg(15);
        }
    }
    else
    {
        ee_printf("USB initialization failed.\n");
    }
    ee_printf("---------\n");

    // Initialise the interrupt handler
    uart_init_irq();

    // Bus raider setup
    br_init();

    // Start the main loop
    main_loop();

}
