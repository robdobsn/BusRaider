// Bus Raider
// Rob Dobson 2020

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "TargetCPUZ80.h"
#include "DebugHelper.h"

// Module name
static const char MODULE_PREFIX[] = "TargCtrlDebug";

// #define DEBUG_REGISTER_GET
// #define DEBUG_M1_CYCLE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugger grab registers and memory
// Returns true if processor should be held at this point
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetControl::debuggerHandleWaitCycle(uint32_t addr, uint32_t data, uint32_t rawBusVals)
{
    // Check if we are debugging
    if (_debuggerState == DEBUGGER_STATE_FREE_RUNNING)
        return false;

    // Instructions to get register values
    static uint8_t regQueryInstructions[] = 
    {
                        // loop:
        0xF5,           //     push af
        0xED, 0x5F,     //     ld a,r - note that this instruction puts IFF2 into PF flag
        0x77,           //     ld (hl),a
        0xED, 0x57,     //     ld a,i
        0x77,           //     ld (hl),a
        0x33,           //     inc sp
        0x33,           //     inc sp
        0x12,           //     ld (de),a
        0x02,           //     ld (bc),a
        0xD9,           //     exx
        0x77,           //     ld (hl),a
        0x12,           //     ld (de),a
        0x02,           //     ld (bc),a
        0xD9,           //     exx
        0x08,           //     ex af,af'
        0xF5,           //     push af
        0x33,           //     inc sp
        0x33,           //     inc sp
        0x08,           //     ex af,af'
        0xDD, 0xE5,     //     push ix        
        0x33,           //     inc sp
        0x33,           //     inc sp    
        0xFD, 0xE5,     //     push iy - no inc sp as we pop af lower down which restores sp to where it was    
        0x3E, 0x00,     //     ld a, 0 - note that the value 0 gets changed in the code below  
        0xED, 0x4F,     //     ld r, a
        0xF1, 0x00, 0x00,    //     pop af + two bytes that are read as if from stack 
                        //     - note that the values 0, 0 get changed in the code below  
        0x18, 0xDE      //     jr loop
    };
    const int RegisterRUpdatePos = 28;
    const int RegisterAFUpdatePos = 32;
    const int RelJumpBackStartPos = 34;

    // Ignore this if not an instruction fetch cycle
    if (((rawBusVals & BR_V20_M1_BAR_MASK) != 0) || ((rawBusVals & BR_MREQ_BAR_MASK) != 0))
    {
        return false;
    }
#ifdef DEBUG_M1_CYCLE
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "DEBUG M1 addr %04x D %02x flags %c %c %c %c %c %08x", 
            addr,
            data,
            (rawBusVals & BR_V20_M1_BAR_MASK) ? ' ' : '1',
            (rawBusVals & BR_RD_BAR_MASK) ? ' ' : 'R',
            (rawBusVals & BR_WR_BAR_MASK) ? ' ' : 'W',
            (rawBusVals & BR_MREQ_BAR_MASK) ? ' ' : 'M',
            (rawBusVals & BR_IORQ_BAR_MASK) ? ' ' : 'I',
            rawBusVals);
#endif

    // Set the PAGE line to disable RAM and allow injection
    digitalWrite(BR_PAGING_RAM_PIN, 0);

    // Iterate intructions
    uint32_t writeIdx = 0;
    uint32_t snippetIdx = 0;
    while (snippetIdx < sizeof(regQueryInstructions))
    {
        // Check if writing
        uint32_t injectVal = 0;
        if ((rawBusVals & BR_WR_BAR_MASK) == 0)
        {
            // Store the result in registers
            switch(snippetIdx)
            {
                case 1:  // push af
                {
                    if (writeIdx == 0)
                    {
                        _z80Registers.SP = addr+1;
                        _z80Registers.AF = data << 8;
                        regQueryInstructions[RegisterAFUpdatePos+1] = data;                     
                        writeIdx++;
                    }
                    else
                    {
                        _z80Registers.AF = (_z80Registers.AF & 0xff00) | (data & 0xff);
                        regQueryInstructions[RegisterAFUpdatePos] = data;                     
                    }
                    break;
                }
                case 4: // ld (hl), a (where a has the contents of r)
                {
                    _z80Registers.HL = addr;
                    // R value compensates for the push af and ld r,a instructions
                    _z80Registers.R = (data + 256 - 3) & 0x7f;
                    // Value stored back to R compensates for ld a,NN and jr loop instructions
                    regQueryInstructions[RegisterRUpdatePos] = (_z80Registers.R + 256 - 2) & 0x7f;
                    break;
                }
                case 7: // ld (hl), a (where a has the contents of i)
                {
                    _z80Registers.I = data;                        
                    break;
                }
                case 10: // ld (de), a
                {
                    _z80Registers.DE = addr;                        
                    break;
                }
                case 11: // ld (bc), a
                {
                    _z80Registers.BC = addr;                        
                    break;
                }
                case 13: // ld (hl), a (after EXX so hl')
                {
                    _z80Registers.HLDASH = addr;                        
                    break;
                }
                case 14: // ld (de), a (after EXX so de')
                {
                    _z80Registers.DEDASH = addr;                        
                    break;
                }
                case 15: // ld (bc), a (after EXX so bc')
                {
                    _z80Registers.BCDASH = addr;                        
                    break;
                }
                case 18:  // push af (actually af')
                {
                    if (writeIdx == 0)
                    {
                        _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff) | (data << 8);
                        writeIdx++;
                    }
                    else
                    {
                        _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff00) | (data & 0xff);
                    }
                    break;
                }
                case 23:  // push ix
                {
                    if (writeIdx == 0)
                    {
                        _z80Registers.IX = (_z80Registers.IX & 0xff) | (data << 8);
                        writeIdx++;
                    }
                    else
                    {
                        _z80Registers.IX = (_z80Registers.IX & 0xff00) | (data & 0xff);
                    }
                    break;
                }
                case 27:  // push ix
                {
                    if (writeIdx == 0)
                    {
                        _z80Registers.IY = (_z80Registers.IY & 0xff) | (data << 8);
                        writeIdx++;
                    }
                    else
                    {
                        _z80Registers.IY = (_z80Registers.IY & 0xff00) | (data & 0xff);
                    }
                    break;
                }
            }

#ifdef DEBUG_REGISTER_GET
            char regsStr[200];
            _z80Registers.format(regsStr, sizeof(regsStr));
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "regGet WR pos %d addr %04x D %02x flags %c %c %c %c %c %08x regs %s", 
                    snippetIdx,
                    addr,
                    data,
                    (rawBusVals & BR_V20_M1_BAR_MASK) ? ' ' : '1',
                    (rawBusVals & BR_RD_BAR_MASK) ? ' ' : 'R',
                    (rawBusVals & BR_WR_BAR_MASK) ? ' ' : 'W',
                    (rawBusVals & BR_MREQ_BAR_MASK) ? ' ' : 'M',
                    (rawBusVals & BR_IORQ_BAR_MASK) ? ' ' : 'I',
                    rawBusVals,
                    regsStr);
#endif
            // Clear the wait
            BusRawAccess::waitResetFlipFlops();    
        }
        else
        {
            // First element of snippet so store PC
            if (snippetIdx == 0)
                _z80Registers.PC = addr;

            // Send instructions to query registers
            injectVal = regQueryInstructions[snippetIdx];
            writeIdx = 0;

#ifdef DEBUG_REGISTER_GET
            char regsStr[200];
            _z80Registers.format(regsStr, sizeof(regsStr));
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "regGet RD pos %d addr %04x D %02x flags %c %c %c %c %c %08x regs %s", 
                    snippetIdx,
                    addr,
                    injectVal,
                    (rawBusVals & BR_V20_M1_BAR_MASK) ? ' ' : '1',
                    (rawBusVals & BR_RD_BAR_MASK) ? ' ' : 'R',
                    (rawBusVals & BR_WR_BAR_MASK) ? ' ' : 'W',
                    (rawBusVals & BR_MREQ_BAR_MASK) ? ' ' : 'M',
                    (rawBusVals & BR_IORQ_BAR_MASK) ? ' ' : 'I',
                    rawBusVals,
                    regsStr);
#endif
            // Move to next read instruction
            snippetIdx++;

            // Check if we're at the point where we should do a memory grab
            bool memoryGrab = false;
            if (snippetIdx == RelJumpBackStartPos)
            {
                // Request the bus
                digitalWrite(BR_BUSRQ_BAR, 0);
                memoryGrab = true;
            }

            // Set data direction out on the data bus driver
            write32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
            _busControl.bus().pibSetOut();

            // Set the value for the processor to read
            _busControl.bus().pibSetValue(injectVal & 0xff);

            // Clear the wait
            BusRawAccess::waitResetFlipFlops();
            
            // Stay here until read cycle is complete
            cycleWaitForReadCompletion();

            // Check for memory grab
            if (memoryGrab)
            {
                // Wait for BUSACK
                uint32_t waitForBUSACKStartUs = micros();
                bool busAckOk = false;
                while(!isTimeout(micros(), waitForBUSACKStartUs, MAX_WAIT_FOR_DEBUG_BUSACK_US))
                {
                    // Get raw GPIO pin values
                    rawBusVals = read32(ARM_GPIO_GPLEV0);

                    // Check BUSACK
                    if ((rawBusVals & BR_BUSACK_BAR_MASK) == 0)
                    {
                        busAckOk = true;
                        break;
                    }
                }

                // Grab memory contents
                if (busAckOk)
                {
                    // Clear the PAGE line to re-enable RAM
                    digitalWrite(BR_PAGING_RAM_PIN, 1);
    
                    // Take bus and set to input
                    BusRawAccess::pibSetIn();
                    write32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);

                    // Control bus
                    BusRawAccess::setPinOut(BR_WR_BAR, 1);
                    BusRawAccess::setPinOut(BR_RD_BAR, 1);
                    BusRawAccess::setPinOut(BR_MREQ_BAR, 1);
                    BusRawAccess::setPinOut(BR_IORQ_BAR, 1);
                    BusRawAccess::setPinOut(BR_WAIT_BAR_PIN, 1);
                    BusRawAccess::setPinOut(BR_V20_M1_BAR, 1);

                    // Grab memory contents
                    uint8_t* memContents = new uint8_t[STD_TARGET_MEMORY_LEN];
                    _busControl.bus().rawBlockRead(0, memContents, STD_TARGET_MEMORY_LEN, BLOCK_ACCESS_MEM);

                    // Send to mem controller
                    _busControl.mem().memCacheBlockWrite(0, memContents, STD_TARGET_MEMORY_LEN, BLOCK_ACCESS_MEM);

                    // Clean up
                    delete memContents;

                    // Release bus
                    // Set M1 high (and other control lines)
                    write32(ARM_GPIO_GPSET0, BR_V20_M1_BAR_MASK | BR_IORQ_BAR_MASK | BR_RD_BAR_MASK | BR_WR_BAR_MASK | BR_MREQ_BAR_MASK);
                    // Pulse MREQ to prime the FF
                    // For V2.0 hardware this also clears the FF that controls data bus output enables (i.e. disables data bus output)
                    // but that doesn't work on V1.7 hardware as BUSRQ holds the FF active
                    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
                    write32(ARM_GPIO_GPCLR0, BR_MREQ_BAR_MASK);
                    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
                    write32(ARM_GPIO_GPSET0, BR_MREQ_BAR_MASK);

                    // Reset WAIT FFs as they may have become set while in BUSRQ/BUSAK
                    BusRawAccess::waitResetFlipFlops();    

                    // Clear BUSRQ
                    write32(ARM_GPIO_GPSET0, BR_BUSRQ_BAR_MASK | BR_V20_M1_BAR_MASK | BR_IORQ_BAR_MASK | BR_RD_BAR_MASK | BR_WR_BAR_MASK | BR_MREQ_BAR_MASK | BR_WAIT_BAR_MASK);

                    // Control bus lines to input
                    pinMode(BR_WR_BAR, INPUT);
                    pinMode(BR_RD_BAR, INPUT);
                    pinMode(BR_MREQ_BAR, INPUT);
                    pinMode(BR_IORQ_BAR, INPUT);
                    pinMode(BR_WAIT_BAR_PIN, INPUT);

                    // Wait until BUSACK is released
                    for (uint32_t j = 0; j < MAX_WAIT_FOR_DEBUG_BUSACK_US; j++)
                    {
                        if (!BusRawAccess::rawBUSAKActive())
                            break;
                        microsDelay(1);
                    }

                    // Assert the PAGE line to disable RAM again
                    digitalWrite(BR_PAGING_RAM_PIN, 0);
                }

                // Release BUSRQ
                digitalWrite(BR_BUSRQ_BAR, 1);
            }
        }

        // Clear output and setup for fast wait
        cycleSetupForFastWait();

        // Delay to ensure previous wait cycle completes
        microsDelay(1);

        // Stay here until next WAIT cycle starts
        uint32_t waitForWAITStartUs = micros();
        while(!isTimeout(micros(), waitForWAITStartUs, MAX_WAIT_FOR_DEBUG_WAIT_US))
        {
            // Get raw GPIO pin values
            rawBusVals = read32(ARM_GPIO_GPLEV0);

            // Check control bus is stable
            if (CTRL_BUS_IS_WAIT(rawBusVals))
                break;
        }
        
        // Read address, data and control busses
        _busControl.bus().addrAndDataBusRead(addr, data);
    }

    // Clear the PAGE line to re-enable RAM
    digitalWrite(BR_PAGING_RAM_PIN, 1);

#ifdef DEBUG_REGISTER_GET
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "regGet done");
#endif

    return true;
}
