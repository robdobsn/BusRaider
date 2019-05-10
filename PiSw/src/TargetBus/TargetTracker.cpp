// Bus Raider
// Rob Dobson 2019

#include "TargetTracker.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "../Hardware/HwManager.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromTargetTracker[] = "TargetTracker";

// Sockets
int TargetTracker::_busSocketId = -1;

// Bus socket
BusSocketInfo TargetTracker::_busSocketInfo = 
{
    .enabled=false,
    TargetTracker::handleWaitInterruptStatic,
    TargetTracker::busActionCompleteStatic,
    .waitOnMemory=true,
    .waitOnIO=false,
    BR_BUS_ACTION_NONE,
    1,
    .busMasterRequest=false,
    .busMasterReason=BR_BUS_ACTION_DISPLAY,
    .holdInWaitReq=false
};

// Code snippet
uint32_t TargetTracker::_snippetLen = 0;
uint32_t TargetTracker::_snippetPos = 0;
uint8_t TargetTracker::_snippetBuf[MAX_CODE_SNIPPET_LEN];
uint32_t TargetTracker::_snippetWriteIdx = 0;

// Injection type
bool TargetTracker::_setRegs = false;

// Mirror memory requirements
bool TargetTracker::_postInjectMemoryMirror = true;

// Disable pending
bool TargetTracker::_disablePending = false;

// Registers
Z80Registers TargetTracker::_z80Registers;

// Prefix tracking
bool TargetTracker::_prefixTracker[2] = {false,false};

// Target state
TargetTracker::TARGET_STATE_ACQ TargetTracker::_targetStateAcqMode = TARGET_STATE_ACQ_NONE;

// Hold at next instruction
TargetTracker::STEP_MODE_TYPE TargetTracker::_stepMode = STEP_MODE_STEP_INTO;

// Debug
uint8_t TargetTracker::_debugInstrBytes[TargetTracker::MAX_BYTES_IN_INSTR];
uint32_t TargetTracker::_debugInstrBytePos = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init, Service, Enable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetTracker::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);
}

// Service
void TargetTracker::service()
{

}

//Enable
void TargetTracker::enable(bool en)
{
    _stepMode = STEP_MODE_STEP_INTO;
    if (en)
    {
        BusAccess::busSocketEnable(_busSocketId, en);
    }
    else
    {
        // Remove any hold
        BusAccess::waitHold(_busSocketId, false);
        _disablePending = true;
    }
}

// Check if bus can be accessed directly
bool TargetTracker::busAccessAvailable()
{
    return !(BusAccess::waitIsHeld() || HwManager::getMemoryEmulationMode() || BusAccess::busSocketIsEnabled(_busSocketId));
}

bool TargetTracker::isPaused()
{
    if (!BusAccess::busSocketIsEnabled(_busSocketId))
        return false;
    return _stepMode == STEP_MODE_STEP_INTO;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetTracker::startGetRegisterSequence()
// {
//     // Start sequence of getting registers
//     _injectionEnabled = true;
//     _gettingRegs = true;
//     _snippetWriteIdx = 0;
//     _snippetPos = 0;
//     _pagingPending = true;
// }

void TargetTracker::startSetRegisterSequence(Z80Registers* pRegs)
{
    // Set regs
    if (pRegs)
        _z80Registers = *pRegs;

    // TODO probably don't need synch with instruction to ensure we are at starting M1 cycle
    // as there is a nop at the start of the sequence
    // Use the bus socket to request page out
    BusAccess::targetPageForInjection(_busSocketId, true);

    // Start sequence of setting registers
    _setRegs = true;
    _snippetPos = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle register setting
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetTracker::store16BitVal(uint8_t arry[], int offset, uint16_t val)
{
    arry[offset] = val & 0xff;
    arry[offset+1] = (val >> 8) & 0xff;
}

int TargetTracker::getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
{
    // Instructions to set register values
    static uint8_t regSetInstructions[] = 
    {
        0x00,                       // nop - in case previous instruction was prefixed
        0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
        0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xd9,                       // exx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x08,                       // ex af,af'
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x47,                 // ld i, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x4f,                 // ld r, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x46,                 // im 0
        0xfb,                       // ei
        0xc3, 0x00, 0x00            // jp xxxx
    };
    const int RegisterIXUpdatePos = 3;
    const int RegisterIYUpdatePos = 7;
    const int RegisterHLDASHUpdatePos = 10;
    const int RegisterDEDASHUpdatePos = 13;
    const int RegisterBCDASHUpdatePos = 16;
    const int RegisterHLUpdatePos = 20;
    const int RegisterDEUpdatePos = 23;
    const int RegisterBCUpdatePos = 26;
    const int RegisterAFDASHUpdatePos = 29;
    const int RegisterAFUpdatePos = 33;
    const int RegisterSPUpdatePos = 36;
    const int RegisterIUpdatePos = 39;
    const int RegisterRUpdatePos = 43;
    const int RegisterAUpdatePos = 47;
    const int RegisterIMUpdatePos = 49;
    const int RegisterINTENUpdatePos = 50;
    const int RegisterPCUpdatePos = 52;

    // Fill in the register values
    store16BitVal(regSetInstructions, RegisterIXUpdatePos, regs.IX);
    store16BitVal(regSetInstructions, RegisterIYUpdatePos, regs.IY);
    store16BitVal(regSetInstructions, RegisterHLDASHUpdatePos, regs.HLDASH);
    store16BitVal(regSetInstructions, RegisterDEDASHUpdatePos, regs.DEDASH);
    store16BitVal(regSetInstructions, RegisterBCDASHUpdatePos, regs.BCDASH);
    store16BitVal(regSetInstructions, RegisterSPUpdatePos, regs.SP);
    store16BitVal(regSetInstructions, RegisterHLUpdatePos, regs.HL);
    store16BitVal(regSetInstructions, RegisterDEUpdatePos, regs.DE);
    store16BitVal(regSetInstructions, RegisterBCUpdatePos, regs.BC);
    store16BitVal(regSetInstructions, RegisterAFDASHUpdatePos, regs.AFDASH);
    store16BitVal(regSetInstructions, RegisterAFUpdatePos, regs.AF);
    regSetInstructions[RegisterIUpdatePos] = regs.I;
    regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 5) % 256;
    regSetInstructions[RegisterAUpdatePos] = regs.AF >> 8;
    regSetInstructions[RegisterIMUpdatePos] = (regs.INTMODE == 0) ? 0x46 : ((regs.INTMODE == 1) ? 0x56 : 0x5e);
    regSetInstructions[RegisterINTENUpdatePos] = (regs.INTENABLED == 0) ? 0xf3 : 0xfb;
    store16BitVal(regSetInstructions, RegisterPCUpdatePos, regs.PC);

    if (codeMaxlen >= sizeof(regSetInstructions))
    {
        memcpy(pCodeBuffer, regSetInstructions, codeMaxlen);
        return sizeof(regSetInstructions);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger - handle register GET
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TargetTracker::OPCODE_INJECT_PROGRESS TargetTracker::handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal)
{
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

    // Check if writing
    if (flags & BR_CTRL_BUS_WR_MASK)
    {
        retVal = BR_MEM_ACCESS_INSTR_INJECT;
        // Store the result in registers
        switch(_snippetPos)
        {
            case 1:  // push af
            {
                if (_snippetWriteIdx == 0)
                {
                    _z80Registers.SP = addr+1;
                    _z80Registers.AF = data << 8;
                    regQueryInstructions[RegisterAFUpdatePos+1] = data;                     
                    _snippetWriteIdx++;
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
                _z80Registers.R = data - 3;
                // Value stored back to R compensates for ld a,NN and jr loop instructions
                regQueryInstructions[RegisterRUpdatePos] = _z80Registers.R - 2;                 
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
                if (_snippetWriteIdx == 0)
                {
                    _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff) | (data << 8);
                    _snippetWriteIdx++;
                }
                else
                {
                    _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff00) | (data & 0xff);
                }
                break;
            }
            case 23:  // push ix
            {
                if (_snippetWriteIdx == 0)
                {
                    _z80Registers.IX = (_z80Registers.IX & 0xff) | (data << 8);
                    _snippetWriteIdx++;
                }
                else
                {
                    _z80Registers.IX = (_z80Registers.IX & 0xff00) | (data & 0xff);
                }
                break;
            }
            case 27:  // push ix
            {
                if (_snippetWriteIdx == 0)
                {
                    _z80Registers.IY = (_z80Registers.IY & 0xff) | (data << 8);
                    _snippetWriteIdx++;
                }
                else
                {
                    _z80Registers.IY = (_z80Registers.IY & 0xff00) | (data & 0xff);
                }
                break;
            }
        }
    }
    else
    {
        if (_snippetPos == 0)
            _z80Registers.PC = addr;
        // Send instructions to query registers
        retVal = regQueryInstructions[_snippetPos++] | BR_MEM_ACCESS_INSTR_INJECT;
        _snippetWriteIdx = 0;
    }

    // Check if complete
    if (_snippetPos >= sizeof(regQueryInstructions))
    {
        _snippetPos = 0;
        return OPCODE_INJECT_DONE;
    }
    else if (_snippetPos == RelJumpBackStartPos)
    {
        return OPCODE_INJECT_GRAB_MEMORY;
    }
    return OPCODE_INJECT_GENERAL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger - handle register SET
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TargetTracker::OPCODE_INJECT_PROGRESS TargetTracker::handleRegisterSet(uint32_t& retVal)
{
    // Fill in the register values
    if (_snippetPos == 0)
    {
        _snippetLen = getInstructionsToSetRegs(_z80Registers, _snippetBuf, MAX_REGISTER_SET_CODE_LEN);
        if (_snippetLen == 0)
        {
            // Nothing to do
            return OPCODE_INJECT_DONE;
        }
    }

    // Return the instruction / data to inject
    retVal = _snippetBuf[_snippetPos++] | BR_MEM_ACCESS_INSTR_INJECT;

    // Check if complete
    if (_snippetPos >= _snippetLen)
        return OPCODE_INJECT_DONE;
    return OPCODE_INJECT_GENERAL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause at next instruction start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetTracker::stepInto()
{
    // Set flag to indicate mode
    _stepMode = STEP_MODE_STEP_INTO;

    // Release bus hold if held
    if (BusAccess::waitIsHeld())
        BusAccess::waitRelease();
}

void TargetTracker::stepRun()
{
    // Set flag to indicate mode
    _stepMode = STEP_MODE_STEP_RUN;

    // Release bus hold if held
    if (BusAccess::waitIsHeld())
        BusAccess::waitRelease();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetTracker::getRegsFormatted(char* pBuf, int len)
{
    _z80Registers.format(pBuf, len);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test if opcode is an instruction prefix
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool TargetTracker::isPrefixInstruction(uint32_t instr)
{
    switch(instr)
    {
        case 0xdd:
        case 0xed:
        case 0xfd:
        case 0xcb:
            return true;
    }
    return false;
}

bool TargetTracker::trackPrefixedInstructions(uint32_t flags, uint32_t codeVal)
{
    // Track M1 cycles to ensure we know when we're at the start of a new instruction
    // and not in the middle of a prefixed instruction sequence
    bool firstByteOfInstr = false;
    if (flags & BR_CTRL_BUS_M1_MASK)
    {
        // Record state from previous instruction
        _prefixTracker[0] = _prefixTracker[1];
        if (!_prefixTracker[0])
            firstByteOfInstr = true;
        // Check if this is a prefixed instruction
        _prefixTracker[1] = isPrefixInstruction(codeVal);
    }
    else
    {
        // Can't be in the middle of a prefixed instruction if this isn't an M1 cycle
        _prefixTracker[0] = _prefixTracker[1] = false;
    }
    return firstByteOfInstr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetTracker::handlePendingDisable()
{
    if (_disablePending)
    {
        _disablePending = false;

        // Disable
        BusAccess::busSocketEnable(_busSocketId, false);

        // Release bus hold
        BusAccess::waitHold(_busSocketId, false);

        // Remove paging for injection
        BusAccess::targetPageForInjection(_busSocketId, false);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle bus actions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetTracker::busActionCompleteStatic([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
    if (actionType == BR_BUS_ACTION_RESET)
    {
        _prefixTracker[0] = _prefixTracker[1] = false;

        // Since we're receiving a reset we know that we're at the start of an instruction
        _targetStateAcqMode = TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle wait callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetTracker::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    // Only handle MREQs
    if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
        return;

    // Value read/written
    uint32_t codeByteValue = ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff;

    // LogWrite(FromTargetTracker, LOG_DEBUG, "M1 %d Prev %d Now %d Dat %02x 1st %d flags %08x", 
    //             flags & BR_CTRL_BUS_M1_MASK, _prefixTracker[0], _prefixTracker[1], data, firstByteOfInstr, flags);

    // digitalWrite(BR_PAGING_RAM_PIN, firstByteOfInstr);
    // microsDelay(1);
    // digitalWrite(BR_PAGING_RAM_PIN, 0);

    // // Debug
    // if (!_injectionAwaitFirst || !firstByteOfInstr)
    // {
    //     if (firstByteOfInstr || (_debugInstrBytePos >= MAX_BYTES_IN_INSTR))
    //         _debugInstrBytePos = 0;
    //     _debugInstrBytes[_debugInstrBytePos++] = codeByteValue;
    // }

    // digitalWrite(BR_PAGING_RAM_PIN, 1);
    // microsDelay(1);
    // digitalWrite(BR_PAGING_RAM_PIN, 0);
    // microsDelay(1);

    // Handle state machine
    switch (_targetStateAcqMode)
    {
        case TARGET_STATE_ACQ_NONE:
        {
            // Check for disable
            if (handlePendingDisable())
                return;

            // If we detect the first byte of an instruction then move state
            bool firstNonPrefixedByteOfInstr = trackPrefixedInstructions(flags, codeByteValue);
            if (firstNonPrefixedByteOfInstr)
                _targetStateAcqMode = TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT;
            // LogWrite("FromTargetTracker", LOG_DEBUG, "NONE %s %04x %02x %s%s %d %d", firstNonPrefixedByteOfInstr ? "FIRSTNONPFX" : "",
            //                 addr, codeByteValue, 
            //                 (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "",
            //                 _prefixTracker[0], _prefixTracker[1]);

            // Debug
            if (firstNonPrefixedByteOfInstr || (_debugInstrBytePos >= MAX_BYTES_IN_INSTR))
                _debugInstrBytePos = 0;
            _debugInstrBytes[_debugInstrBytePos++] = codeByteValue;
            break;
        }
        case TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT:
        {
            // Check for disable
            if (handlePendingDisable())
                return;

            // Remove any hold to allow execution / injection
            BusAccess::waitHold(_busSocketId, false);

            // If we're waiting for the instruction to finish then make sure this is a first byte
            if (_targetStateAcqMode == TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT)
            {
                // If we get another first byte then start injecting
                bool firstNonPrefixedByteOfInstr = trackPrefixedInstructions(flags, codeByteValue);
                if (!firstNonPrefixedByteOfInstr)
                {
                    // LogWrite("FromTargetTracker", LOG_DEBUG, "WAITFIRST %04x %02x %s%s %d %d", 
                    //         addr, codeByteValue, (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "",
                    //         _prefixTracker[0], _prefixTracker[1]);
                    // Debug
                    if (_debugInstrBytePos < MAX_BYTES_IN_INSTR)
                        _debugInstrBytes[_debugInstrBytePos++] = codeByteValue;
                    return;
                }
                // Use the bus socket to request page out
                // LogWrite("FromTargetTracker", LOG_DEBUG, "targetPageForInjection TRUE");
                BusAccess::targetPageForInjection(_busSocketId, true);

                // Bump state
                _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;
            }
            // NOTE: this falls through to the next case statement intentionally
            [[fallthrough]];
        }
        // fall through
        case TARGET_STATE_ACQ_INJECTING:
        {
            // Handle get or set
            OPCODE_INJECT_PROGRESS injectProgress = OPCODE_INJECT_GENERAL;
            if (_setRegs)
                injectProgress = handleRegisterSet(retVal);
            else
                injectProgress = handleRegisterGet(addr, data, flags, retVal);

            // Check if time for memory grab
            if (injectProgress == OPCODE_INJECT_GRAB_MEMORY)
            {
                // Check if post-inject memory mirroring required
                if (_postInjectMemoryMirror)
                {

                    // Suspend bus detail after a BUSRQ as there is a hardware problem with FF_DATA_OE_BAR remaining
                    // enabled after a BUSRQ which causes contention issues on the PIB. The way around this is to
                    // ensure BUSRQ is handled synchronously with the TargetTracker operation when MREQ waits are
                    // enabled so that the cycle immediately following a BUSRQ in this case will be part of the 
                    // opcode injection cycle and the bus detail will not be required
                    BusAccess::waitSuspendBusDetailOneCycle();

                    // Request bus
                    // LogWrite("FromTargetTracker", LOG_DEBUG, "Request bus for mirror");
                    BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_MIRROR);
                }
            }
            else if (injectProgress == OPCODE_INJECT_DONE)
            {
                // Default back to getting
                _setRegs = false;

                // Next state
                _targetStateAcqMode = TARGET_STATE_ACQ_POST_INJECT;

                // Use the bus socket to request page-in delayed to next wait event
                BusAccess::targetPageForInjection(_busSocketId, false);

                // LogWrite("FromTargetTracker", LOG_DEBUG, "INJECTING FINISHED 0x%04x 0x%02x %s%s",
                //             addr, ((flags & BR_CTRL_BUS_RD_MASK) & ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0)) ? (retVal & 0xff) : data,  
                //             (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

                // TODO
                // char regBuf[1000];
                // _z80Registers.format(regBuf, 1000);
                // char tmpBuf[10];
                // for (uint32_t i = 0; i < _debugInstrBytePos; i++)
                // {    
                //     ee_sprintf(tmpBuf, " %02x", _debugInstrBytes[i]);
                //     strlcat(regBuf, tmpBuf, 1000);
                // }
                // LogWrite("FromTargetTracker", LOG_DEBUG, "%s", regBuf);

                // TODO may need to call back to whoever requested this
            }
            else
            {
                // LogWrite("FromTargetTracker", LOG_DEBUG, "INJECTING %04x %02x %s%s",
                //             addr, ((flags & BR_CTRL_BUS_RD_MASK) & ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0)) ? (retVal & 0xff) : data, 
                //             (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

            }
            break;
        }
        case TARGET_STATE_ACQ_POST_INJECT:
        {
            // Tracking back on
            trackPrefixedInstructions(flags, codeByteValue);

            // If pending disable then go to disabled
            if (handlePendingDisable())
            {
                // LogWrite("FromTargetTracker", LOG_DEBUG, "Removed");
                _targetStateAcqMode = TARGET_STATE_ACQ_NONE;
                return;
            }

            // Check for hold required
            if (_stepMode == STEP_MODE_STEP_INTO)
            {
                // Tell bus to hold at this point
                BusAccess::waitHold(_busSocketId, true);
            }

            // Go back to first byte got
            _targetStateAcqMode = TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT;

            // Debug
            // LogWrite("FromTargetTracker", LOG_DEBUG, "POST-INJECT %04x %02x %s%s",
            //             addr, ((flags & BR_CTRL_BUS_RD_MASK) & ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0)) ? (retVal & 0xff) : data, 
            //             (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

            break;
        }
    }
        // LogWrite("FromTargetTracker", LOG_DEBUG, "INJECTING %s %04x %02x %s%s %d %d", (_targetStateAcqMode = TARGET_STATE_ACQ_NONE) ? "FINISHED" : "",
        //     addr, retVal & 0xff, (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

    // // Await first byte of instruction fetch
    // if ((_injectionAwaitFirst && !firstByteOfInstr))
    // {
    //     LogWrite("FromTargetTracker", LOG_DEBUG, "ACT AWAIT %s %04x %02x %s%s %d %d", firstByteOfInstr ? "FIRST" : "",
    //             addr, codeByteValue, (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "", _prefixTracker[0], _prefixTracker[1]);
    //     return;
    // }
    // _injectionAwaitFirst = false;

    // TODO
    // Start of paging
    // if (_pagingPending)
    // {
    //     // Use the bus socket to request page out
    //     BusAccess::targetPageForInjection(_busSocketId, true, false);
    //     _pagingPending = false;
    // }

    // digitalWrite(BR_PAGING_RAM_PIN, 1);
    // microsDelay(5);
    // digitalWrite(BR_PAGING_RAM_PIN, 0);
    // microsDelay(5);

    // Handle get
    // if (_gettingRegs)
    //     handleRegisterGet(addr, data, flags, retVal);
    // else
    //     handleRegisterSet(retVal);

    // LogWrite("FromTargetTracker", LOG_DEBUG, "ACT %s %s %04x %02x %s%s %d %d", firstByteOfInstr ? "FIRST" : "", !_injectionActive ? "GOINACT" : "",
    //     addr, retVal & 0xff, (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "", _prefixTracker[0], _prefixTracker[1]);

    // // Check if still active
    // if (!_injectionActive)
    // {
    //     // TODO
    //     // Use the bus socket to request page-in delayed to next wait event
    //     // BusAccess::targetPageForInjection(_busSocketId, false, true);

    //     // TODO
    //     char regBuf[1000];
    //     _z80Registers.format(regBuf, 1000);

    //     char tmpBuf[10];
    //     for (uint32_t i = 0; i < _debugInstrBytePos; i++)
    //     {    
    //         ee_sprintf(tmpBuf, " %02x", _debugInstrBytes[i]);
    //         strlcat(regBuf, tmpBuf, 1000);
    //     }
    //     LogWrite("FromTargetTracker", LOG_DEBUG, "%s", regBuf);

    //     // Default back to getting
    //     _gettingRegs = true;

    //     // If pending disable then go to disabled
    //     if (_disablePending)
    //         BusAccess::busSocketEnable(_busSocketId, false);
    //     _disablePending = false;

    //     // TODO may need to call back to whoever requested this
    // }
    
}

