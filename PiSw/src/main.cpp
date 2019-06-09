// Bus Raider
// Rob Dobson 2018-2019

#include "System/lowlev.h"
#include "System/lowlib.h"
#include "System/piwiring.h"
#include "System/UartMaxi.h"
#include "System/ee_sprintf.h"
#include "System/timer.h"
#include "System/logging.h"
#include "System/Display.h"
#include "TargetBus/BusAccess.h"
#include "TargetBus/TargetTracker.h"
#include "Hardware/HwManager.h"
#include "Machines/McManager.h"
#include "BusController/BusController.h"
#include "ZEsarUXInterface/ZEsarUXInterface.h"
#include "StepTracer/StepTracer.h"
#include "BusRaiderApp.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Program details
static const char* PROG_VERSION = "Bus Raider V1.7.132 (C) Rob Dobson 2018-2019";
static const char* PROG_LINKS_1 = "https://robdobson.com/tag/raider";

// Send log data to display (as opposed to merging in the ESP32 log output)
// #define LOG_TO_DISPLAY 1

// Log string
const char* FromMain = "Main";

// Display
Display display;

// Baud rate
#define MAIN_UART_BAUD_RATE 921600
UartMaxi mainUart;

// CommandHandler, StepTracer, BusController
StepTracer stepTracer;
BusController busController;
ZEsarUXInterface _ZEsarUXInterface;
McManager mcManager;

// Bus Raider app
BusRaiderApp busRaiderApp(display, mainUart);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug to display
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void debugToDisplay(const char* pSeverity, const char* pSource, const char* pMsg)
{
    display.logDebug(pSeverity, pSource, pMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" int main()
{
    // Initialise UART
    if (!mainUart.setup(MAIN_UART_BAUD_RATE, 1000000, 32768))
    {
        // display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, "ESP32: Not Connected, UART Fail");
        microsDelay(5000000);
    }

    // Logging
    LogSetLevel(LOG_DEBUG);
    LogSetOutMsgFn(CommandHandler::logDebug);
    LogWrite(FromMain, LOG_NOTICE, "Startup ...");

    // Init timers
    timers_init();

    // Initialise graphics system
    display.init();

    // Status
    display.statusPut(Display::STATUS_FIELD_PI_VERSION, Display::STATUS_NORMAL, PROG_VERSION);
    display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, "ESP32 Not Connected");
    display.statusPut(Display::STATUS_FIELD_LINKS, Display::STATUS_NORMAL, PROG_LINKS_1);

    #ifdef LOG_TO_DISPLAY
    LogSetOutMsgFn(debugToDisplay);
    #endif

    // Bus raider setup
    BusAccess::init();

    // Hardware manager
    HwManager::init();

    // Target tracker
    TargetTracker::init();

    // BusController, StepTracer
    busController.init();
    stepTracer.init();
    _ZEsarUXInterface.init();

    // Init machine manager
    mcManager.init(&display);

    // USB and status
    busRaiderApp.initUSB();

    // Bus raider app
    busRaiderApp.init();

    // Select Serial Terminal machine - overridden with info from ESP32
    mcManager.setMachineByName("Serial Terminal");


    LogWrite(FromMain, LOG_DEBUG, "StepTracer %08x %u ZEsarUXInterface %08x %u",
            &stepTracer, &stepTracer, &_ZEsarUXInterface, &_ZEsarUXInterface);

    // Loop forever
    while(1)
    {
        // Handle target machine display updates
        McManager::displayRefresh();

        // Service the comms channels and display updates
        busRaiderApp.service();

        // Timer polling
        timer_poll();

        // Service bus access
        BusAccess::service();

        // Service hardware manager
        HwManager::service();

        // Target tracker
        TargetTracker::service();

        // Service machine manager
        McManager::service();

        // BusController, StepTracer
        busController.service();
        stepTracer.service();
        _ZEsarUXInterface.service();
    }
}
