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
#include "StepValidator/StepValidator.h"
#include "BusRaiderApp.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Program details
static const char* PROG_VERSION = "Bus Raider V1.7.112 (C) Rob Dobson 2018-2019";
static const char* PROG_LINKS_1 = "https://robdobson.com/tag/raider";

// Log string
const char* FromMain = "Main";

// Display
Display display;

// Baud rate
#define MAIN_UART_BAUD_RATE 921600
UartMaxi mainUart;

// CommandHandler, StepValidator, BusController
StepValidator stepValidator;
BusController busController;
ZEsarUXInterface _ZEsarUXInterface;
McManager mcManager;

// Bus Raider app
BusRaiderApp busRaiderApp(display, mainUart);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" int main()
{
    // Initialise UART
    if (!mainUart.setup(MAIN_UART_BAUD_RATE, 1000000, 100000))
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

    // Bus raider setup
    BusAccess::init();

    // Hardware manager
    HwManager::init();

    // Target tracker
    TargetTracker::init();

    // BusController, StepValidator
    busController.init();
    stepValidator.init();
    _ZEsarUXInterface.init();

    // Init machine manager
    mcManager.init(&display);

    // USB and status
    busRaiderApp.initUSB();

    // Bus raider app
    busRaiderApp.init();

    // Select Serial Terminal machine - overridden with info from ESP32
    mcManager.setMachineByName("Serial Terminal");

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

        // BusController, StepValidator
        busController.service();
        stepValidator.service();
        _ZEsarUXInterface.service();
    }
}
