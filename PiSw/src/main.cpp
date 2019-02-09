// Bus Raider
// Rob Dobson 2018

#include "System/uart.h"
#include "System/wgfx.h"
#include "System/ee_printf.h"
#include "TargetBus/piwiring.h"
#include "TargetBus/BusAccess.h"
#include "TargetBus/target_memory_map.h"
#include "CommandInterface/CommandHandler.h"
#include "Utils/rdutils.h"
#include "Machines/McManager.h"
#include "Machines/McTRS80.h"
#include "Machines/McRobsZ80.h"
#include "Machines/McDebugZ80.h"
#include "Machines/McZXSpectrum.h"
#include "Machines/McTerminal.h"
#include "System/timer.h"
#include "System/lowlib.h"
#include "System/lowlev.h"
#include <stdlib.h>

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef char                s8;
typedef short               s16;
typedef int                 s32;

// #include "../uspi\include\uspi\types.h"
#include "../uspi/include/uspi.h"

// Program details
static const char* PROG_VERSION = "                    Bus Raider V1.7.011";
static const char* PROG_CREDITS = "                        Rob Dobson 2018";
static const char* PROG_LINKS_1 = "       https://robdobson.com/tag/raider";
static const char* PROG_LINKS_2 = "https://github.com/robdobsn/PiBusRaider";

// Baud rate
#define MAIN_UART_BAUD_RATE 115200

// Immediate mode
#define IMM_MODE_LINE_MAXLEN 100
bool _immediateMode = false;
char _immediateModeLine[IMM_MODE_LINE_MAXLEN+1];
int _immediateModeLineLen = 0;

// Command Handler
CommandHandler commandHandler;

// Function to send to uart from command handler
void putToSerial(const uint8_t* pBuf, int len)
{
    for (int i = 0; i < len; i++)
        uart_send(pBuf[i]);
}

void serviceGetFromSerial()
{
    // Handle serial communication
    for (int rxCtr = 0; rxCtr < 100; rxCtr++) {
        if (!uart_poll())
            break;
        // Handle char
        int ch = uart_read_byte();
        uint8_t buf[2];
        buf[0] = ch;
        commandHandler.handleSerialReceivedChars(buf, 1);
    }    
}

void layout_display()
{
    // Clear display
    wgfx_clear();
    
    // Layout display
    McDescriptorTable* pMcDescr = McManager::getDescriptorTable(0);
    int windowBorderWidth = 5;
    wgfx_set_window(0, -1, 0, 
        pMcDescr->displayPixelsX, pMcDescr->displayPixelsY,
        pMcDescr->displayCellX, pMcDescr->displayCellY,
        pMcDescr->pixelScaleX, pMcDescr->pixelScaleY,
        pMcDescr->pFont, pMcDescr->displayForeground, pMcDescr->displayBackground,
        windowBorderWidth, 8);
    wgfx_set_window(1, 0, (pMcDescr->displayPixelsY*pMcDescr->pixelScaleY) + windowBorderWidth*2 + 10, 
        -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        0, 8);
    wgfx_set_console_window(1);
}

extern "C" void set_machine_by_name(const char* mcName)
{
    // Set the machine
    if (McManager::setMachineByName(mcName))
    {
        // Set the display
        layout_display();
    }

}

static void _keypress_raw_handler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // Check for immediate mode
    if (rawKeys[0] == KEY_F2)
    {
        if (!_immediateMode)
        {
            ee_printf("Entering immediate mode, e.g. w/ssid/password/hostname<enter> to setup WiFi ...\n");
        }
        _immediateMode = true;
        return;
    }
    if (_immediateMode)
    {
        if (_immediateModeLineLen < IMM_MODE_LINE_MAXLEN)
        {
            int asciiCode = McTerminal::convertRawToAscii(ucModifiers, rawKeys);
            if (asciiCode == 0)
                return;
            if (asciiCode == 0x08)
            {
                if (_immediateModeLineLen > 0)
                    _immediateModeLineLen--;
                wgfx_term_putchar(asciiCode);
                wgfx_term_putchar(' ');
            }
            else if (asciiCode == 0x0d)
            {
                _immediateMode = false;
                _immediateModeLine[_immediateModeLineLen] = 0;
                if (_immediateModeLineLen > 0)
                {
                    commandHandler.sendAPIReq(_immediateModeLine);
                    ee_printf("Sent request to ESP32: %s\n", _immediateModeLine);
                }
                _immediateModeLineLen = 0;
            }
            else if ((asciiCode >= 32) && (asciiCode < 127))
            {
                _immediateModeLine[_immediateModeLineLen++] = asciiCode;
            }
            wgfx_term_putchar(asciiCode);
            // ee_printf("%x ", asciiCode);
        }
        return;
    }

    // Send to the target machine to process
    // ee_printf("KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
    McBase* pMc = McManager::getMachine();
    if (pMc)
        pMc->keyHandler(ucModifiers, rawKeys);
}

extern "C" int main()
{

    // Logging
    LogSetLevel(LOG_DEBUG);
    
    // Heap init
    heapInit();

    // Init timers
    timers_init();

    // UART
    uart_init(MAIN_UART_BAUD_RATE, 1);

    // Command handler
    commandHandler.setMachineChangeCallback(set_machine_by_name);
    commandHandler.setPutToSerialCallback(putToSerial);

    // Target machine memory and command handler
    targetClear();

    // Init machine manager
    McManager::init(&commandHandler);

    // Add machines
    new McTerminal();
    new McTRS80();
    new McRobsZ80();
    new McDebugZ80();
    new McZXSpectrum();

    // Initialise graphics system
    wgfx_init(1366, 768);

    // Layout display for the selected machine
    layout_display();

    // Number of machines
    ee_printf("%d machines supported\n", McManager::getNumMachines());

    // Bus raider setup
    BusAccess::init();

    // Enable first machine
    McManager::setMachineIdx(1);

    // Get current machine to check things are working
    if (!McManager::getMachine())
    {
        ee_printf("Failed to construct default machine\n");
    }

    // USB
    if (USPiInitialize()) 
    {
        ee_printf("Checking for keyboards...");

        if (USPiKeyboardAvailable()) 
        {
            USPiKeyboardRegisterKeyStatusHandlerRaw(_keypress_raw_handler);
            ee_printf("keyboard found\n");
        } else 
        {
            wgfx_set_fg(9);
            ee_printf("keyboard not found\n");
            wgfx_set_fg(15);
        }
    } else 
    {
        ee_printf("USB initialization failed\n");
    }
    ee_printf("\nPress F2 for immediate mode\n");

    // Debug show colour palette
    // for (int i = 0; i < 255; i++)
    // {
    //     wgfx_set_fg(i);
    //     ee_printf("%02d ", i);
    // }
    // wgfx_set_fg(15);

    // Waiting...
    // ee_printf("Waiting for UART data (%d,8,N,1)\n", MAIN_UART_BAUD_RATE);

    // Refresh rate
    McDescriptorTable* pMcDescr = McManager::getDescriptorTable(0);
    const unsigned long reqUpdateUs = 1000000 / pMcDescr->displayRefreshRatePerSec;
    #define REFRESH_RATE_WINDOW_SIZE_MS 2000
    int refreshCount = 0;
    unsigned long curRateSampleWindowStart = micros();
    unsigned long lastDisplayUpdateUs = 0;
    unsigned long dispTime = 0;
    bool lastActivityTickerState = false;

    // Status update rate and last timer
    unsigned long lastStatusUpdateMs = 0;
    const unsigned long STATUS_UPDATE_RATE_MS = 3000;

// #define TEST_VFP
#ifdef TEST_VFP

    volatile uint32_t lastMicros = micros();
        // Loop forever
    while (1) 
    {

        volatile uint32_t newMicros = micros();
        // volatile uint32_t lastModMicros = newMicros;
        if (newMicros > lastMicros)
        {
            if (newMicros - lastMicros > 5)
            // if (newMicros % 1000 < lastModMicros)
            {
                // while (1)
                // {
                // for (int i = 0; i < 100; i++)
                    digitalWrite(BR_DEBUG_PI_SPI0_CE0,1);
                    lowlev_cycleDelay(1000);
                // for (int i = 0; i < 100; i++)
                    digitalWrite(BR_DEBUG_PI_SPI0_CE0,0);
                    lowlev_cycleDelay(1000);
                // }
            }
            // lastModMicros = newMicros % 1000;
        }
        digitalWrite(BR_HADDR_CK, 1);
        lowlev_cycleDelay(10);
        digitalWrite(BR_HADDR_CK, 0);
        lowlev_cycleDelay(10);
        lastMicros = micros();

     }
#else

    // Loop forever
    while (1) 
    {

        // Handle target machine display updates
        if (isTimeout(micros(), lastDisplayUpdateUs, reqUpdateUs)) 
        {
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
        if (isTimeout(micros(), curRateSampleWindowStart, REFRESH_RATE_WINDOW_SIZE_MS * 1000)) 
        {
            // Initial message
            wgfx_set_fg(11); // 11 = yellow
            int lineIdx = 0;
            wgfx_puts(1, wgfx_get_term_width()-strlen(PROG_VERSION)-1, lineIdx++, (uint8_t*)PROG_VERSION);
            wgfx_puts(1, wgfx_get_term_width()-strlen(PROG_CREDITS)-1, lineIdx++, (uint8_t*)PROG_CREDITS);
            wgfx_puts(1, wgfx_get_term_width()-strlen(PROG_LINKS_1)-1, lineIdx++, (uint8_t*)PROG_LINKS_1);
            wgfx_puts(1, wgfx_get_term_width()-strlen(PROG_LINKS_2)-1, lineIdx++, (uint8_t*)PROG_LINKS_2);

            // Show ESP health info
            bool ipAddrValid = false;
            char* ipAddr = NULL;
            char* wifiStatusChar = NULL;
            char* wifiSSID = NULL;
            commandHandler.getStatusResponse(&ipAddrValid, &ipAddr, &wifiStatusChar, &wifiSSID);
            const int MAX_STATUS_STR_LEN = 50;
            char statusStr[MAX_STATUS_STR_LEN+1];
            statusStr[0] = 0;
            switch(*wifiStatusChar)
            {
                case 'C': 
                    strlcpy(statusStr, "WiFi ", MAX_STATUS_STR_LEN); 
                    if (ipAddrValid)
                    {
                        strlcpy(statusStr+strlen(statusStr), ipAddr, MAX_STATUS_STR_LEN);
                    }
                    break;
                default: 
                    strlcpy(statusStr, "WiFi not connected", MAX_STATUS_STR_LEN);
                    break;
            }
            wgfx_puts(1, wgfx_get_term_width()-strlen(statusStr)-1, lineIdx++, (uint8_t*)statusStr);

            // Refresh rate
            int refreshRate = refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
            const int MAX_REFRESH_STR_LEN = 40;
            const char* refreshText = "Refresh ";
            char refreshStr[MAX_REFRESH_STR_LEN+1] = "  ";
            refreshStr[0] = lastActivityTickerState ? '|' : '-';
            lastActivityTickerState = !lastActivityTickerState;
            strlcat(refreshStr, refreshText, MAX_REFRESH_STR_LEN);
            uint8_t rateStr[20];
            rditoa(refreshRate, rateStr, MAX_REFRESH_STR_LEN, 10);
            strlcat(refreshStr, (char*)rateStr, MAX_REFRESH_STR_LEN);
            strlcat(refreshStr, "fps", MAX_REFRESH_STR_LEN);
            wgfx_puts(1, wgfx_get_term_width()-strlen(refreshStr)-1, lineIdx++, (uint8_t*)refreshStr);
            // // uart_printf("Rate %d per sec, requs %ld dispTime %ld\n", refreshCount / 2, reqUpdateUs, dispTime);
            // wgfx_putc(1, 150, 0, '0' + (refreshRate % 10));
            // wgfx_putc(1, 149, 0, '0' + ((refreshRate / 10) % 10));
            // if (refreshRate / 100 != 0)
            //     wgfx_putc(1, 148, 0, '0' + ((refreshRate / 100) % 10));

            // Ready for next time
            refreshCount = 0;
            curRateSampleWindowStart = micros();
            wgfx_set_fg(15);
        }

        // Handle status update to ESP32
        if (isTimeout(micros(), lastStatusUpdateMs, STATUS_UPDATE_RATE_MS * 1000)) 
        {
            // Send and request status update
            commandHandler.requestStatusUpdate();
            lastStatusUpdateMs = micros();
        }

        // Pump the characters from the UART to Command Handler
        serviceGetFromSerial();

        // Service command handler
        commandHandler.service();

        // Timer polling
        timer_poll();

        // Service bus raider
        BusAccess::service();

        // Service target debugger
        TargetDebug* pDebug = TargetDebug::get();
        if (pDebug)
            pDebug->service();
    }
#endif
}

