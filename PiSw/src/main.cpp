// Bus Raider
// Rob Dobson 2018

#include "System/UartMini.h"
#include "System/UartMaxi.h"
#include "System/Display.h"
#include "System/ee_printf.h"
#include "System/piwiring.h"
#include "TargetBus/BusAccess.h"
#include "TargetBus/TargetState.h"
#include "CommandInterface/CommandHandler.h"
#include "System/OTAUpdate.h"
#include "System/rdutils.h"
#include "Machines/McManager.h"
#include "Machines/McTRS80.h"
#include "Machines/McRobsZ80.h"
#include "Machines/McZXSpectrum.h"
#include "Machines/McTerminal.h"
#include "Hardware/HwManager.h"
#include "Hardware/Hw512KRamRom.h"
#include "Hardware/Hw64KPagedRAM.h"
#include "System/timer.h"
#include "System/lowlib.h"
#include "System/lowlev.h"
#include "Machines/usb_hid_keys.h"
#include <stdlib.h>

typedef unsigned char		u8;
#include "../uspi/include/uspi.h"

// Program details
static const char* PROG_VERSION = "Bus Raider V1.7.073 (C) Rob Dobson 2018-2019";
static const char* PROG_LINKS_1 = "https://robdobson.com/tag/raider";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log string
const char* FromMain = "Main";

// Display
Display display;

// Baud rate
#define MAIN_UART_BAUD_RATE 500000
UartMaxi mainUart;

// CommandHandler
CommandHandler commandHandler;

// Immediate mode
#define IMM_MODE_LINE_MAXLEN 100
bool _immediateMode = false;
char _immediateModeLine[IMM_MODE_LINE_MAXLEN+1];
int _immediateModeLineLen = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void uartWriteString(const char* pStr)
{
    while (*pStr)
    {
        mainUart.write(*pStr++);
    }
}

void consolePutString(const char* pStr)
{
    display.consolePut(pStr);
}

// Function to send to uart from command handler
void handlePutToSerialForCmdHandler(const uint8_t* pBuf, int len)
{
    for (int i = 0; i < len; i++)
        mainUart.write(pBuf[i]);
}

int errorCounts[UartMaxi::UART_ERROR_FULL+1];

void serviceGetFromSerial()
{
    UartMaxi::UART_ERROR_CODES errCode = mainUart.getStatus();
    if (errCode != UartMaxi::UART_ERROR_NONE)
    {
        if (errCode <= UartMaxi::UART_ERROR_FULL)
            errorCounts[errCode]++;
    }
    
    // Handle serial communication
    for (int rxCtr = 0; rxCtr < 10000; rxCtr++) {
        if (!mainUart.poll())
            break;
        // Handle char
        int ch = mainUart.read();
        uint8_t buf[2];
        buf[0] = ch;
        CommandHandler::handleSerialReceivedChars(buf, 1);
    }    
}

extern "C" void setMachineOptions(const char* mcOpts)
{
    // Set the machine
    McManager::setMachineOpts(mcOpts);
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
                display.consolePut(asciiCode);
                display.consolePut(' ');
            }
            else if (asciiCode == 0x0d)
            {
                _immediateMode = false;
                _immediateModeLine[_immediateModeLineLen] = 0;
                if (_immediateModeLineLen > 0)
                {
                    CommandHandler::sendAPIReq(_immediateModeLine);
                    ee_printf("Sent request to ESP32: %s\n", _immediateModeLine);
                }
                _immediateModeLineLen = 0;
            }
            else if ((asciiCode >= 32) && (asciiCode < 127))
            {
                _immediateModeLine[_immediateModeLineLen++] = asciiCode;
            }
            display.consolePut(asciiCode);
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status Display
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Refresh rate
#define REFRESH_RATE_WINDOW_SIZE_MS 2000
static int refreshCount = 0;
static unsigned long curRateSampleWindowStart = micros();
static unsigned long lastDisplayUpdateUs = 0;
static unsigned long dispTime = 0;
static bool lastActivityTickerState = false;

// Status update rate and last timer
static unsigned long lastStatusUpdateMs = 0;
static const unsigned long STATUS_UPDATE_RATE_MS = 3000;

void statusDisplayUpdate()
{
    // Update display refresh rate
    if (isTimeout(micros(), curRateSampleWindowStart, REFRESH_RATE_WINDOW_SIZE_MS * 1000)) 
    {
        // Get ESP health info
        bool ipAddrValid = false;
        char* ipAddr = NULL;
        char* wifiStatusChar = NULL;
        char* wifiSSID = NULL;
        char* esp32Version = NULL;
        commandHandler.getStatusResponse(&ipAddrValid, &ipAddr, &wifiStatusChar, &wifiSSID, &esp32Version);

        // ESP32 info
        const int MAX_STATUS_STR_LEN = 150;
        char statusStr[MAX_STATUS_STR_LEN+1];
        strlcpy(statusStr, "ESP32 Version: ", MAX_STATUS_STR_LEN);
        if (strlen(esp32Version) == 0)
        {
            strlcat(statusStr, "Not Connected!", MAX_STATUS_STR_LEN);
            display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, statusStr);
        }
        else
        {
            strlcat(statusStr, esp32Version, MAX_STATUS_STR_LEN);
            display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_NORMAL, statusStr);
        }

        // WiFi status
        statusStr[0] = 0;
        switch(*wifiStatusChar)
        {
            case 'C': 
                strlcpy(statusStr, "WiFi IP: ", MAX_STATUS_STR_LEN); 
                if (ipAddrValid)
                    strlcat(statusStr, ipAddr, MAX_STATUS_STR_LEN);
                display.statusPut(Display::STATUS_FIELD_IP_ADDR, Display::STATUS_NORMAL, statusStr);
                break;
            default: 
                strlcpy(statusStr, "WiFi not connected", MAX_STATUS_STR_LEN);
                display.statusPut(Display::STATUS_FIELD_IP_ADDR, Display::STATUS_FAIL, statusStr);
                break;
        }

        // Machine name
        strlcpy(statusStr, "M/C: ", MAX_STATUS_STR_LEN);
        strlcat(statusStr, McManager::getDescriptorTable()->machineName, MAX_STATUS_STR_LEN);
        display.statusPut(Display::STATUS_FIELD_CUR_MACHINE, Display::STATUS_NORMAL, statusStr);

        // Speed
        int clockSpeed = McManager::getMachineClock();
        int mhz = clockSpeed/1000000;
        int khz = (clockSpeed - mhz*1000000)/1000;
        ee_sprintf(statusStr, "Clock: %d.%s%s%dMHz",
                    mhz,
                    khz <= 99 ? "0" : "",
                    khz <= 9 ? "0" : "",
                    khz);
        // strlcpy(statusStr, "Clock: ", MAX_STATUS_STR_LEN);
        // ltoa(mhz, statusStr+strlen(statusStr), 10);
        // strlcat(statusStr, ".", MAX_STATUS_STR_LEN);
        // if (khz <= 99)
        //     strlcat(statusStr, "0", MAX_STATUS_STR_LEN);
        // if (khz <= 9)
        //     strlcat(statusStr, "0", MAX_STATUS_STR_LEN);
        // ltoa(khz, statusStr+strlen(statusStr), 10);
        // strlcat(statusStr, "MHz", MAX_STATUS_STR_LEN);
        display.statusPut(Display::STATUS_FIELD_MACHINES, Display::STATUS_NORMAL, statusStr);

        // BusAccess status
        statusStr[0] = 0;
        strlcpy(statusStr, "Bus: ", MAX_STATUS_STR_LEN);
        if (McManager::targetIsPaused())
            strlcat(statusStr, "Paused", MAX_STATUS_STR_LEN);
        else
            strlcat(statusStr, "Free Running", MAX_STATUS_STR_LEN);
        if (McManager::targetBusUnderPiControl())
            strlcat(statusStr, "PiControl", MAX_STATUS_STR_LEN);
        display.statusPut(Display::STATUS_FIELD_BUS_ACCESS, Display::STATUS_NORMAL, statusStr);

        // Refresh rate
        int refreshRate = refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
        const int MAX_REFRESH_STR_LEN = 40;
        const char* refreshText = "Refresh: ";
        char refreshStr[MAX_REFRESH_STR_LEN+1];
        strlcpy(refreshStr, lastActivityTickerState ? "| " : "- ", MAX_REFRESH_STR_LEN);
        lastActivityTickerState = !lastActivityTickerState;
        strlcat(refreshStr, refreshText, MAX_REFRESH_STR_LEN);
        uint8_t rateStr[20];
        rditoa(refreshRate, rateStr, MAX_REFRESH_STR_LEN, 10);
        strlcat(refreshStr, (char*)rateStr, MAX_REFRESH_STR_LEN);
        strlcat(refreshStr, "fps", MAX_REFRESH_STR_LEN);
        display.statusPut(Display::STATUS_FIELD_REFRESH_RATE, Display::STATUS_NORMAL, refreshStr);

        // Get ISR debug info
        strcpy(statusStr, "");
        for (int i = 0; i < ISR_ASSERT_NUM_CODES; i++)
        {
            int cnt = BusAccess::isrAssertGetCount(i);
            if (cnt > 0)
            {
                ee_sprintf(refreshStr, "[%d]=%d,", i, cnt);
                strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
            }
        }
        if (strlen(statusStr) > 0)
            display.statusPut(Display::STATUS_FIELD_ASSERTS, Display::STATUS_FAIL, statusStr);

        // Ready for next time
        refreshCount = 0;
        curRateSampleWindowStart = micros();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" int main()
{
    // Init timers
    timers_init();

    // Initialise graphics system
    display.init();

    // Status
    display.statusPut(Display::STATUS_FIELD_PI_VERSION, Display::STATUS_NORMAL, PROG_VERSION);
    display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, "ESP32 Not Connected");
    display.statusPut(Display::STATUS_FIELD_LINKS, Display::STATUS_NORMAL, PROG_LINKS_1);

    // Initialise UART
    if (!mainUart.setup(MAIN_UART_BAUD_RATE, 100000, 1000))
    {
        display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, "ESP32: Not Connected, UART Fail");
        microsDelay(5000000);
    }

    // Logging
    LogSetLevel(LOG_DEBUG);
    LogSetOutFn(consolePutString);

    // Command Handler
    commandHandler.setMachineCommandCallback(McManager::handleCommand);
    commandHandler.setPutToSerialCallback(handlePutToSerialForCmdHandler);
    commandHandler.setOTAUpdateCallback(OTAUpdate::performUpdate);
    commandHandler.setTargetFileCallback(McManager::handleTargetFile);

    // Target machine memory
    TargetState::clear();

    // Init hardware manager
    HwManager::init(&commandHandler, &display);

    // Add hardware
    new Hw512KRamRom();
    new Hw64KPagedRam();

    // Init machine manager
    McManager::init(&commandHandler, &display);

    // Add machines
    new McTerminal();
    new McTRS80();
    new McRobsZ80();
    new McZXSpectrum();

    // Number of machines
    // static const int MAX_STATUS_STR_LEN = 200;
    // char statusStr[MAX_STATUS_STR_LEN];
    // ee_sprintf(statusStr, "Machines Supported: %d", McManager::getNumMachines());
    // display.statusPut(Display::STATUS_FIELD_MACHINES, Display::STATUS_NORMAL, statusStr);

    // Bus raider setup
    BusAccess::init();

    // Enable first machine
    McManager::setMachineIdx(1, 0, true);

    // Get current machine to check things are working
    static const int MAX_STATUS_STR_LEN = 200;
    char statusStr[MAX_STATUS_STR_LEN];
    if (!McManager::getMachine())
    {
        strlcat(statusStr, ", Failed to Start Default", MAX_STATUS_STR_LEN);
        display.statusPut(Display::STATUS_FIELD_MACHINES, Display::STATUS_FAIL, statusStr);
    }

    // USB
    if (USPiInitialize()) 
    {
        // LogWrite(FromMain, LOG_DEBUG, "Checking for keyboards...");

        if (USPiKeyboardAvailable()) 
        {
            USPiKeyboardRegisterKeyStatusHandlerRaw(_keypress_raw_handler);
            display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_NORMAL, "Keyboard OK, F2 for Settings");
        } 
        else 
        {
            display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_FAIL, "Keyboard Not Found");
        }
    } else 
    {
        display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_NORMAL, "USB Init Fail - No Keyboard");
    }

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

    // Request machine info from ESP32
    CommandHandler::sendAPIReq("querycurmc");
    CommandHandler::sendAPIReq("querycuropts");

    // Loop forever
    while (1) 
    {

        // Handle target machine display updates
        McDescriptorTable* pMcDescr = McManager::getDescriptorTable();
        unsigned long reqUpdateUs = 1000000 / pMcDescr->displayRefreshRatePerSec;
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

        // Update status display
        statusDisplayUpdate();

        // Handle status update to ESP32
        if (isTimeout(micros(), lastStatusUpdateMs, STATUS_UPDATE_RATE_MS * 1000)) 
        {
            // Send and request status update
            commandHandler.sendRegularStatusUpdate();
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

        // Service hardware manager
        HwManager::service();

        // Service machine manager
        McManager::service();

        // Service target debugger
        TargetDebug::get()->service();
    }
#endif
}

