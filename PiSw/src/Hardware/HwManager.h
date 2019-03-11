// Bus Raider Hardware Manager
// Rob Dobson 2019

#pragma once

#include "../System/ee_printf.h"
#include <string.h> 
#include "../System/RingBufferPosn.h"
#include "../CommandInterface/CommandHandler.h"
#include "../System/Display.h"
#include "HwBase.h"

#define DEBUG_IO_ACCESS 1

#ifdef DEBUG_IO_ACCESS
class DebugIOPortAccess
{
public:
    int port;
    bool type;
    int val;
};
#endif

class HwManager
{
private:

    static const int MAX_HARDWARE = 10;
    static const int MAX_HARDWARE_NAME_LEN = 100;
    static HwBase* _pHw[MAX_HARDWARE];
    static int _numHardware;

    // Command handler
    static CommandHandler* _pCommandHandler;

    // Display
    static Display* _pDisplay;

#ifdef DEBUG_IO_ACCESS
    // Debug collection of IO accesses
    static const int DEBUG_MAX_IO_PORT_ACCESSES = 500;
    static DebugIOPortAccess _debugIOPortBuf[DEBUG_MAX_IO_PORT_ACCESSES];
    static RingBufferPosn _debugIOPortPosn;
#endif

public:
    // Init
    static void init(CommandHandler* pCommandHandler, Display* pDisplay);

    // Service
    static void service();

    // Manage machines
    static void add(HwBase* pHw);

    // Interrupt handler for MREQ/IORQ
    static uint32_t handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags);

    // Page out RAM/ROM for opcode injection
    static void pageOutRamRom(bool restore);

    // Check if paging requires bus access
    static bool pagingRequiresBusAccess();
};
