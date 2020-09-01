// Bus Raider
// Rob Dobson 2018-2019

#include "System/lowlev.h"
#include "System/lowlib.h"
#include "System/PiWiring.h"
#include "System/UartMaxi.h"
#include "System/ee_sprintf.h"
#include "System/timer.h"
#include "System/logging.h"
#include "System/Display.h"
#include "TargetBus/BusAccess.h"
#include "TargetBus/TargetTracker.h"
#include "TargetBus/TargetProgrammer.h"
#include "Hardware/HwManager.h"
#include "Machines/McManager.h"
#include "BusController/BusController.h"
#include "DeZogInterface/DeZogInterface.h"
#include "StepTracer/StepTracer.h"
#include "BusRaiderApp.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Program details
const char* PROG_VERSION = "Bus Raider V2.2.62  (C) Rob Dobson 2018-2019";
static const char* PROG_LINKS_1 = "https://robdobson.com/tag/raider";

// Send log data to display (as opposed to merging in the ESP32 log output)
// #define LOG_TO_DISPLAY 1

// Log string
const char* FromMain = "Main";

// Display
Display display;

// Main UART
UartMaxi mainUart;

// CommandHandler, StepTracer, BusController
CommandHandler commandHandler;
McManager mcManager(commandHandler);
StepTracer stepTracer(commandHandler, mcManager);
BusController busController(commandHandler, mcManager);
DeZogInterface deZogInterface(commandHandler, mcManager);
BusAccess busAccess;
TargetTracker targetTracker(mcManager);
TargetProgrammer targetProgrammer;
HwManager hwManager(commandHandler, mcManager);

// Bus Raider app
BusRaiderApp busRaiderApp(display, mainUart, commandHandler, mcManager);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug to display
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void debugToDisplay(const char* pSeverity, const char* pSource, const char* pMsg)
{
    display.logDebug(pSeverity, pSource, pMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug to ESP32
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void debugToESP32(const char* pSeverity, const char* pSource, const char* pMsg)
{
    commandHandler.logDebug(pSeverity, pSource, pMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" int main()
{
    // Initialise UART
    if (!mainUart.setup(busRaiderApp.getDefaultAutoBaudRate(), 1000000, 32768))
    {
        // display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, "ESP32: Not Connected, UART Fail");
        microsDelay(5000000);
    }

    // Logging
    LogSetLevel(LOG_DEBUG);
    LogSetOutMsgFn(debugToESP32);
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
    busAccess.init();

    // Hardware manager
    hwManager.init();

    // Target tracker
    targetTracker.init();

    // Target programmer
    targetProgrammer.init();

    // BusController, StepTracer
    busController.init();
    stepTracer.init();
    deZogInterface.init();

    // Init machine manager
    mcManager.init(&display, hwManager, busAccess, stepTracer, targetTracker, targetProgrammer);

    // USB and status
    busRaiderApp.initUSB();

    // Bus raider app
    busRaiderApp.init();

    // Select Serial Terminal machine - overridden with info from ESP32
    mcManager.setMachineByName("Serial Terminal");


    LogWrite(FromMain, LOG_DEBUG, "StepTracer %08x %u DeZogInterface %08x %u",
            &stepTracer, &stepTracer, &deZogInterface, &deZogInterface);

    // Loop forever
    while(1)
    {
        // Handle target machine display updates
        mcManager.displayRefresh();

        // Service the comms channels and display updates
        busRaiderApp.service();

        // Timer polling
        timer_poll();

        // Service bus access
        busAccess.service();

        // Service hardware manager
        hwManager.service();

        // Target tracker
        targetTracker.service();

        // Target programmer
        targetProgrammer.service();

        // Service machine manager
        mcManager.service();

        // BusController, StepTracer
        busController.service();
        stepTracer.service();
        deZogInterface.service();
    }
}
