// Bus Raider
// Rob Dobson 2018

#include "globaldefs.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
#include "wgfx.h"
#include "ee_printf.h"
#include "piwiring.h"
#include "busraider.h"
#include "cmd_handler.h"
#include "target_memory_map.h"
#include "rdutils.h"
#include "../uspi\include\uspi\types.h"
#include "../uspi/include/uspi.h"
#include "McManager.h"
#include "McTRS80.h"
#include "McRobsZ80.h"

// Baud rate
#define MAIN_UART_BAUD_RATE 115200

static void _keypress_raw_handler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // ee_printf("KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
    McBase* pMc = McManager::getMachine();
    if (pMc)
        pMc->keyHandler(ucModifiers, rawKeys);
}

extern "C" void entry_point()
{

    // Logging
    LogSetLevel(LOG_DEBUG);
    
    // System init
    system_init();

    // Init timers
    timers_init();

    // UART
    uart_init(MAIN_UART_BAUD_RATE, 1);

    // Target machine memory and command handler
    targetClear();
    cmdHandler_init();

    // Add machines
    new McTRS80();
    new McRobsZ80();

    // Enable first machine
    McManager::setMachineIdx(1);

    // Get current machine to check things are working
    if (!McManager::getMachine())
    {
        uart_printf("Failed to construct machine\n");
    }

    // Get machine descriptor table
    McDescriptorTable* pMcDescr = McManager::getDescriptorTable(0);

    // Initialise graphics system
    wgfx_init(1366, 768);

    // Layout display
    int windowBorderWidth = 5;
    wgfx_set_window(0, -1, 0, 
        pMcDescr->displayPixelsX, pMcDescr->displayPixelsY,
        pMcDescr->displayCellX, pMcDescr->displayCellY,
        pMcDescr->pixelScaleX, pMcDescr->pixelScaleY,
        pMcDescr->pFont, pMcDescr->displayForeground, pMcDescr->displayBackground,
        windowBorderWidth, 8);
    wgfx_set_window(1, 0, pMcDescr->displayPixelsY + windowBorderWidth*2 + 10, -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        windowBorderWidth, 8);
    wgfx_set_console_window(1);

    // Initial message
    wgfx_set_fg(11);
    ee_printf("RC2014 Bus Raider V1.01\n");
    wgfx_set_fg(15);
    ee_printf("Rob Dobson 2018 (inspired by PiGFX)\n\n");

    // Number of machines
    ee_printf("%d machines supported\n", McManager::getNumMachines());

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

    // Bus raider setup
    br_init();

    // Bus raider enable wait states
    br_enable_wait_states();

    ee_printf("Waiting for UART data (%d,8,N,1)\n", MAIN_UART_BAUD_RATE);

    const unsigned long reqUpdateUs = 1000000 / pMcDescr->displayRefreshRatePerSec;

    #define REFRESH_RATE_WINDOW_SIZE_MS 2000
    int refreshCount = 0;
    unsigned long curRateSampleWindowStart = micros();
    unsigned long lastDisplayUpdateUs = 0;
    unsigned long dispTime = 0;

    while (1) {

        // Handle target machine display updates
        if (timer_isTimeout(micros(), lastDisplayUpdateUs, reqUpdateUs)) {
            // Check valid
            dispTime = micros();
            lastDisplayUpdateUs = micros();
            McBase* pMc = McManager::getMachine();
            if (pMc)
                pMc->displayRefresh();
            dispTime = micros() - dispTime;
            refreshCount++;
        }

        // Handle refresh rate calculation
        if (timer_isTimeout(micros(), curRateSampleWindowStart, REFRESH_RATE_WINDOW_SIZE_MS * 1000)) {
            int refreshRate = refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
            // uart_printf("Rate %d per sec, requs %ld dispTime %ld\n", refreshCount / 2, reqUpdateUs, dispTime);
            wgfx_putc(1, 150, 0, '0' + (refreshRate % 10));
            wgfx_putc(1, 149, 0, '0' + ((refreshRate / 10) % 10));
            if (refreshRate / 100 != 0)
                wgfx_putc(1, 148, 0, '0' + ((refreshRate / 100) % 10));
            refreshCount = 0;
            curRateSampleWindowStart = micros();
        }

        // Service command handler
        cmdHandler_service();

        // Timer polling
        timer_poll();

        // Service bus raider
        br_service();
    }
}

// #endif