// Bus Raider
// Rob Dobson 2019

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
// #include "TargetCPUZ80.h"

// Constructor
TargetControl::TargetControl(BusControl& busAccess)
    : _busAccess(busAccess)
{
    cycleClear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::init()
{
}

// Service
void TargetControl::service()
{
    // Service processing cycle
    cycleService();
}

// #define INCLUDE_DISASSEMBLER

// #ifdef INCLUDE_DISASSEMBLER
// #include "mdZ80.h"
// #endif

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Variables
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // The code to get registers can pause on the last RD of the get-regs injected code
// // or it can pause on the get of the actual current instruction addr (after get-regs)
// // has completed.
// // Reason for wanting it the old way might be that you can't (easily) change the instruction 
// // at the current address as the processor will read it as soon as the wait is released and 
// // the target-tracker's instruction prefix tracking might also get out-of-kilter if a prefixed 
// // instruction was changed for a non-prefix one etc
// // Reason for wanting the new way is the things that look at the bus while held will get the
// // right value now
// #define PAUSE_GET_REGS_AT_CUR_ADDR 1

// // Module name
// static const char MODULE_PREFIX[] = "TargetControl";

// // Constructor
// TargetControl::TargetControl(BusControl& busAccess)
//     : _busAccess(busAccess)
// {
//     // Sockets
//     _busSocketId = -1;

//     // Code snippet
//     _snippetLen = 0;
//     _snippetPos = 0;
//     _snippetWriteIdx = 0;

//     // Step over
//     _stepOverPCValue = 0;

//     // Injection type
//     _setRegs = false;

//     // Mirror memory requirements
//     _postInjectMemoryMirror = true;

//     // Request grab display while stepping
//     _requestDisplayWhileStepping = false;

//     // Reset pending
//     _targetResetPending = false;

//     // Disable pending
//     _disablePending = false;

//     // Page out for injection active
//     _pageOutForInjectionActive = false;

//     // Prefix tracking
//     _prefixTracker[0] = false;
//     _prefixTracker[1] = false;

//     // Target state
//     _targetStateAcqMode = TARGET_STATE_ACQ_NONE;

//     // Hold at next instruction
//     _stepMode = STEP_MODE_STEP_PAUSED;

//     // Machine heartbeat
//     _machineHeartbeatCounter = 0;

//     // Pending actions
//     _busActionPendingProgramTarget = false;
//     _busActionPendingExecAfterProgram = false;
//     _busActionCodeWrittenAtResetVector = false;

//     // Debug
//     _debugInstrBytePos = 0;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Init, Service, Enable
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::init()
// {
//     // TODO 2020 removed
//     // // Connect to the bus socket
//     // if (_busSocketId < 0)
//     //     _busSocketId = _busAccess.socketAdd(    
//     //         true,
//     //         TargetControl::handleWaitInterruptStatic,
//     //         TargetControl::busActionCompleteStatic,
//     //         false,
//     //         false,
//     //         // Reset
//     //         false,
//     //         0,
//     //         // NMI
//     //         false,
//     //         0,
//     //         // IRQ
//     //         false,
//     //         0,
//     //         false,
//     //         BR_BUS_ACTION_GENERAL,
//     //         false,
//     //         this
//     //     );
// }

// // Service
// void TargetControl::service()
// {

// }

// //Enable
// void TargetControl::enable(bool en, bool waitHold)
// {
//     // TODO 2020
//     // _stepMode = STEP_MODE_STEP_PAUSED;
//     // if (en)
//     // {
//     //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "enable %d", en);
//     //     // Enable the bus socket so we get bus callbacks
//     //     _busAccess.socketEnable(_busSocketId, en);

//     //     // Set mirror mode so we record memory accesses
//     //     // TODO 2020
//     //     // _mcManager.getHwManager().setMirrorMode(true);
//     //     _postInjectMemoryMirror = true;

//     //     // Wait on memory and hold at each instruction
//     //     // TODO 2020
//     //     // _busAccess.waitOnMemory(_busSocketId, true);
//     //     // _busAccess.waitHold(_busSocketId, waitHold);
//     //     // if (!waitHold)
//     //     //     _busAccess.waitRelease();
//     // }
//     // else
//     // {
//     //     // Turn off mirror mode
        
        
//     //     // TODO 2020
//     //     // _mcManager.getHwManager().setMirrorMode(false);
//     //     // Remove any hold
//     //     // _busAccess.waitHold(_busSocketId, false);
//     //     // if (_targetStateAcqMode != TARGET_STATE_ACQ_INJECTING)
//     //     // {
//     //     //     // Disable
//     //     //     _busAccess.waitOnMemory(_busSocketId, false);
//     //     //     _busAccess.socketEnable(_busSocketId, en);

//     //     //     // TODO 2020
//     //     //     // // Remove paging for injection
//     //     //     // _busAccess.targetPageForInjection(_busSocketId, false);
//     //     //     _pageOutForInjectionActive = false;
//     //     // }
//     //     // else
//     //     // {
//     //     //     _disablePending = true;
//     //     // }
//     // }
// }

// bool TargetControl::isPaused()
// {
//     // TODO 2020
//     // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busSocketIsEnabled %d %d", 
//     // //             busAccess.busSocketIsEnabled(_busSocketId), _stepMode);
//     // if (!_busAccess.socketIsEnabled(_busSocketId))
//     //     return false;
//     return _stepMode == STEP_MODE_STEP_PAUSED;
// }

// bool TargetControl::isTrackingActive()
// {
//     // TODO 2020
//     // return _busAccess.socketIsEnabled(_busSocketId);
//     return false;
// }

// void TargetControl::targetReset()
// {
//     _targetResetPending = true;
//     // TODO 2020
//     // _busAccess.socketReqReset(_busSocketId);
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Control
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::startSetRegisterSequence(Z80Registers* pRegs)
// {
//     // TODO 2020
//     // // Set regs
//     // if (pRegs)
//     //     _z80Registers = *pRegs;

//     // // TODO probably don't need synch with instruction to ensure we are at starting M1 cycle
//     // // as there is a nop at the start of the sequence

//     // // Check we can inject
//     // if (_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING)
//     // {
//     //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "Can't inject as Injector is busy - state = %d", _targetStateAcqMode);
//     //     return;
//     // }

//     // // Set state machine to inject
//     // _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;

//     // // Remove any hold to allow execution / injection
//     // _busAccess.waitHold(_busSocketId, false);

//     // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Wait release for injection");

//     // // Start sequence of setting registers
//     // _setRegs = true;
//     // _snippetPos = 0;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle register setting
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::store16BitVal(uint8_t arry[], int offset, uint16_t val)
// {
//     arry[offset] = val & 0xff;
//     arry[offset+1] = (val >> 8) & 0xff;
// }

// int TargetControl::getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
// {
//     // Instructions to set register values
//     static uint8_t regSetInstructions[] = 
//     {
//         0x00,                       // nop - in case previous instruction was prefixed
//         0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
//         0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
//         0x21, 0x00, 0x00,           // ld hl, xxxx
//         0x11, 0x00, 0x00,           // ld de, xxxx
//         0x01, 0x00, 0x00,           // ld bc, xxxx
//         0xd9,                       // exx
//         0x21, 0x00, 0x00,           // ld hl, xxxx
//         0x11, 0x00, 0x00,           // ld de, xxxx
//         0x01, 0x00, 0x00,           // ld bc, xxxx
//         0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
//         0x08,                       // ex af,af'
//         0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
//         0x31, 0x00, 0x00,           // ld sp, xxxx
//         0x3e, 0x00,                 // ld a, xx
//         0xed, 0x47,                 // ld i, a
//         0x3e, 0x00,                 // ld a, xx
//         0xed, 0x4f,                 // ld r, a
//         0x3e, 0x00,                 // ld a, xx
//         0xed, 0x46,                 // im 0
//         0xfb,                       // ei
//         0xc3, 0x00, 0x00            // jp xxxx
//     };
//     const int RegisterIXUpdatePos = 3;
//     const int RegisterIYUpdatePos = 7;
//     const int RegisterHLDASHUpdatePos = 10;
//     const int RegisterDEDASHUpdatePos = 13;
//     const int RegisterBCDASHUpdatePos = 16;
//     const int RegisterHLUpdatePos = 20;
//     const int RegisterDEUpdatePos = 23;
//     const int RegisterBCUpdatePos = 26;
//     const int RegisterAFDASHUpdatePos = 29;
//     const int RegisterAFUpdatePos = 33;
//     const int RegisterSPUpdatePos = 36;
//     const int RegisterIUpdatePos = 39;
//     const int RegisterRUpdatePos = 43;
//     const int RegisterAUpdatePos = 47;
//     const int RegisterIMUpdatePos = 49;
//     const int RegisterINTENUpdatePos = 50;
//     const int RegisterPCUpdatePos = 52;

//     // Fill in the register values
//     store16BitVal(regSetInstructions, RegisterIXUpdatePos, regs.IX);
//     store16BitVal(regSetInstructions, RegisterIYUpdatePos, regs.IY);
//     store16BitVal(regSetInstructions, RegisterHLDASHUpdatePos, regs.HLDASH);
//     store16BitVal(regSetInstructions, RegisterDEDASHUpdatePos, regs.DEDASH);
//     store16BitVal(regSetInstructions, RegisterBCDASHUpdatePos, regs.BCDASH);
//     store16BitVal(regSetInstructions, RegisterSPUpdatePos, regs.SP);
//     store16BitVal(regSetInstructions, RegisterHLUpdatePos, regs.HL);
//     store16BitVal(regSetInstructions, RegisterDEUpdatePos, regs.DE);
//     store16BitVal(regSetInstructions, RegisterBCUpdatePos, regs.BC);
//     store16BitVal(regSetInstructions, RegisterAFDASHUpdatePos, regs.AFDASH);
//     store16BitVal(regSetInstructions, RegisterAFUpdatePos, regs.AF);
//     regSetInstructions[RegisterIUpdatePos] = regs.I;
//     regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 5) % 256;
//     regSetInstructions[RegisterAUpdatePos] = regs.AF >> 8;
//     regSetInstructions[RegisterIMUpdatePos] = (regs.INTMODE == 0) ? 0x46 : ((regs.INTMODE == 1) ? 0x56 : 0x5e);
//     regSetInstructions[RegisterINTENUpdatePos] = (regs.INTENABLED == 0) ? 0xf3 : 0xfb;
//     store16BitVal(regSetInstructions, RegisterPCUpdatePos, regs.PC);

//     if (codeMaxlen >= sizeof(regSetInstructions))
//     {
//         memcopyfast(pCodeBuffer, regSetInstructions, sizeof(regSetInstructions));
//         return sizeof(regSetInstructions);
//     }
//     return 0;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Interrupt extension for debugger - handle register GET
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TargetControl::OPCODE_INJECT_PROGRESS TargetControl::handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal)
// {
//     // Instructions to get register values
//     static uint8_t regQueryInstructions[] = 
//     {
//                         // loop:
//         0xF5,           //     push af
//         0xED, 0x5F,     //     ld a,r - note that this instruction puts IFF2 into PF flag
//         0x77,           //     ld (hl),a
//         0xED, 0x57,     //     ld a,i
//         0x77,           //     ld (hl),a
//         0x33,           //     inc sp
//         0x33,           //     inc sp
//         0x12,           //     ld (de),a
//         0x02,           //     ld (bc),a
//         0xD9,           //     exx
//         0x77,           //     ld (hl),a
//         0x12,           //     ld (de),a
//         0x02,           //     ld (bc),a
//         0xD9,           //     exx
//         0x08,           //     ex af,af'
//         0xF5,           //     push af
//         0x33,           //     inc sp
//         0x33,           //     inc sp
//         0x08,           //     ex af,af'
//         0xDD, 0xE5,     //     push ix        
//         0x33,           //     inc sp
//         0x33,           //     inc sp    
//         0xFD, 0xE5,     //     push iy - no inc sp as we pop af lower down which restores sp to where it was    
//         0x3E, 0x00,     //     ld a, 0 - note that the value 0 gets changed in the code below  
//         0xED, 0x4F,     //     ld r, a
//         0xF1, 0x00, 0x00,    //     pop af + two bytes that are read as if from stack 
//                         //     - note that the values 0, 0 get changed in the code below  
//         0x18, 0xDE      //     jr loop
//     };
//     const int RegisterRUpdatePos = 28;
//     const int RegisterAFUpdatePos = 32;
//     const int RelJumpBackStartPos = 34;

//     // Check if writing
//     if (flags & BR_CTRL_BUS_WR_MASK)
//     {
//         retVal = BR_MEM_ACCESS_INSTR_INJECT;
//         // Store the result in registers
//         switch(_snippetPos)
//         {
//             case 1:  // push af
//             {
//                 if (_snippetWriteIdx == 0)
//                 {
//                     _z80Registers.SP = addr+1;
//                     _z80Registers.AF = data << 8;
//                     regQueryInstructions[RegisterAFUpdatePos+1] = data;                     
//                     _snippetWriteIdx++;
//                 }
//                 else
//                 {
//                     _z80Registers.AF = (_z80Registers.AF & 0xff00) | (data & 0xff);
//                     regQueryInstructions[RegisterAFUpdatePos] = data;                     
//                 }
//                 break;
//             }
//             case 4: // ld (hl), a (where a has the contents of r)
//             {
//                 _z80Registers.HL = addr;
//                 // R value compensates for the push af and ld r,a instructions
//                 _z80Registers.R = data - 3;
//                 // Value stored back to R compensates for ld a,NN and jr loop instructions
//                 regQueryInstructions[RegisterRUpdatePos] = _z80Registers.R - 2;                 
//                 break;
//             }
//             case 7: // ld (hl), a (where a has the contents of i)
//             {
//                 _z80Registers.I = data;                        
//                 break;
//             }
//             case 10: // ld (de), a
//             {
//                 _z80Registers.DE = addr;                        
//                 break;
//             }
//             case 11: // ld (bc), a
//             {
//                 _z80Registers.BC = addr;                        
//                 break;
//             }
//             case 13: // ld (hl), a (after EXX so hl')
//             {
//                 _z80Registers.HLDASH = addr;                        
//                 break;
//             }
//             case 14: // ld (de), a (after EXX so de')
//             {
//                 _z80Registers.DEDASH = addr;                        
//                 break;
//             }
//             case 15: // ld (bc), a (after EXX so bc')
//             {
//                 _z80Registers.BCDASH = addr;                        
//                 break;
//             }
//             case 18:  // push af (actually af')
//             {
//                 if (_snippetWriteIdx == 0)
//                 {
//                     _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff) | (data << 8);
//                     _snippetWriteIdx++;
//                 }
//                 else
//                 {
//                     _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff00) | (data & 0xff);
//                 }
//                 break;
//             }
//             case 23:  // push ix
//             {
//                 if (_snippetWriteIdx == 0)
//                 {
//                     _z80Registers.IX = (_z80Registers.IX & 0xff) | (data << 8);
//                     _snippetWriteIdx++;
//                 }
//                 else
//                 {
//                     _z80Registers.IX = (_z80Registers.IX & 0xff00) | (data & 0xff);
//                 }
//                 break;
//             }
//             case 27:  // push ix
//             {
//                 if (_snippetWriteIdx == 0)
//                 {
//                     _z80Registers.IY = (_z80Registers.IY & 0xff) | (data << 8);
//                     _snippetWriteIdx++;
//                 }
//                 else
//                 {
//                     _z80Registers.IY = (_z80Registers.IY & 0xff00) | (data & 0xff);
//                 }
//                 break;
//             }
//         }
//     }
//     else
//     {
//         if (_snippetPos == 0)
//             _z80Registers.PC = addr;
//         // Send instructions to query registers
//         retVal = regQueryInstructions[_snippetPos++] | BR_MEM_ACCESS_INSTR_INJECT;
//         _snippetWriteIdx = 0;
//     }

//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "regGet pos %d D %02x flags %c %c %c %c %c %02x", 
//     //             _snippetPos-1,
//     //             retVal,
//     //             (flags & BR_CTRL_BUS_M1_MASK) ? '1' : ' ',
//     //             (flags & BR_CTRL_BUS_RD_MASK) ? 'R' : ' ',
//     //             (flags & BR_CTRL_BUS_WR_MASK) ? 'W' : ' ',
//     //             (flags & BR_CTRL_BUS_MREQ_MASK) ? 'M' : ' ',
//     //             (flags & BR_CTRL_BUS_IORQ_MASK) ? 'I' : ' ',
//     //             flags);

//     // Check if complete
//     if (_snippetPos >= sizeof(regQueryInstructions))
//     {
//         _snippetPos = 0;
//         // char regStr[200];
//         // _z80Registers.format(regStr, 200);
//         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Reg values: %s", regStr);
//         return OPCODE_INJECT_DONE;
//     }
//     else if (_snippetPos == RelJumpBackStartPos)
//     {
//         return OPCODE_INJECT_GRAB_MEMORY;
//     }
//     return OPCODE_INJECT_GENERAL;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Interrupt extension for debugger - handle register SET
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TargetControl::OPCODE_INJECT_PROGRESS TargetControl::handleRegisterSet(uint32_t& retVal)
// {
//     // Fill in the register values
//     if (_snippetPos == 0)
//     {
//         _snippetLen = getInstructionsToSetRegs(_z80Registers, _snippetBuf, MAX_REGISTER_SET_CODE_LEN);
//         if (_snippetLen == 0)
//         {
//             // Nothing to do
//             return OPCODE_INJECT_DONE;
//         }
//     }

//     // Return the instruction / data to inject
//     retVal = _snippetBuf[_snippetPos++] | BR_MEM_ACCESS_INSTR_INJECT;

//     // Check if complete
//     if (_snippetPos >= _snippetLen)
//         return OPCODE_INJECT_DONE;
//     return OPCODE_INJECT_GENERAL;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Pause at next instruction start
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::stepInto()
// {
//     // TODO 2020
//     // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepInto");

//     // // Set flag to indicate mode
//     // _stepMode = STEP_MODE_STEP_INTO;

//     // // Release bus hold if held
//     // if (_busAccess.waitIsHeld())
//     // {
//     //     // Remove any hold to allow execution / injection
//     //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepInto waitHold = false");
//     //     _busAccess.waitHold(_busSocketId, false);
//     // }

//     // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Breakpoints en %d num %d", _breakpoints.isEnabled(), _breakpoints.getNumEnabled());
// }

// void TargetControl::stepOver()
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepOver");

//     // Get address to run to by disassembling code at current location

//     // TODO 2020
//     // RESTORE ...
//     // uint8_t* pMirrorMemory = _mcManager.getHwManager().getMirrorMemForAddr(0);
//     // if (!pMirrorMemory)
//     //     return;
//     // uint32_t curAddr = _z80Registers.PC;
//     // char pDisassembly[MAX_Z80_DISASSEMBLY_LINE_LEN];
//     // int instrLen = disasmZ80(pMirrorMemory, 0, curAddr, pDisassembly, INTEL, false, true);
//     // _stepOverPCValue = curAddr + instrLen;
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cpu-step-over PCnow %04x StepToPC %04x", _z80Registers.PC, _stepOverPCValue);

//     // TODO 2020
//     // // Set flag to indicate mode
//     // _stepMode = STEP_MODE_STEP_OVER;

//     // // Release bus hold if held
//     // if (_busAccess.waitIsHeld())
//     // {
//     //     // Remove any hold to allow execution / injection
//     //     _busAccess.waitHold(_busSocketId, false);
//     // }
// }

// void TargetControl::stepTo(uint32_t toAddr)
// {
//     // TODO 2020
//     // _stepOverPCValue = toAddr;
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cpu-step-to PCnow %04x StepToPC %04x", _z80Registers.PC, toAddr);

//     // // Set flag to indicate mode
//     // _stepMode = STEP_MODE_STEP_OVER;

//     // // Release bus hold if held
//     // if (_busAccess.waitIsHeld())
//     // {
//     //     // Remove any hold to allow execution / injection
//     //     _busAccess.waitHold(_busSocketId, false);
//     // }
// }

// void TargetControl::stepRun()
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepRun");

//     // TODO 2020
//     // // Set flag to indicate mode
//     // _stepMode = STEP_MODE_RUN;

//     // // Release bus hold if held
//     // if (_busAccess.waitIsHeld())
//     // {
//     //     // Remove any hold to allow execution / injection
//     //     _busAccess.waitHold(_busSocketId, false);
//     // }
// }

// // void TargetControl::stepPause(bool allowInjection)
// // {
// //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepPause");

// //     // Set flag to indicate mode
// //     _stepMode = STEP_MODE_STEP_PAUSED;

// //     // Release bus hold if held
// //     if (busAccess.waitIsHeld())
// //     {
// //         // Remove any hold to allow execution / injection
// //         busAccess.waitHold(_busSocketId, false);
// //     }
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Status
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::getRegsFormatted(char* pBuf, int len)
// {
//     _z80Registers.format(pBuf, len);
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Test if opcode is an instruction prefix
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// bool TargetControl::isPrefixInstruction(uint32_t instr)
// {
//     switch(instr)
//     {
//         case 0xdd:
//         case 0xed:
//         case 0xfd:
//         case 0xcb:
//             return true;
//     }
//     return false;
// }

// bool TargetControl::trackPrefixedInstructions(uint32_t flags, uint32_t data, uint32_t retVal)
// {
//     // Value read/written
//     uint32_t codeVal = ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff;

//     // Track M1 cycles to ensure we know when we're at the start of a new instruction
//     // and not in the middle of a prefixed instruction sequence
//     bool firstByteOfInstr = false;
//     if (flags & BR_CTRL_BUS_M1_MASK)
//     {
//         // Record state from previous instruction
//         _prefixTracker[0] = _prefixTracker[1];
//         if (!_prefixTracker[0])
//             firstByteOfInstr = true;
//         // Check if this is a prefixed instruction
//         _prefixTracker[1] = isPrefixInstruction(codeVal);
//     }
//     else
//     {
//         // Can't be in the middle of a prefixed instruction if this isn't an M1 cycle
//         _prefixTracker[0] = _prefixTracker[1] = false;
//     }
//     return firstByteOfInstr;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Callbacks/Hooks
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool TargetControl::handlePendingDisable()
// {
//     // TODO 2020
//     // if (_disablePending)
//     // {
//     //     _disablePending = false;

//     //     // Disable
//     //     _busAccess.waitOnMemory(_busSocketId, false);
//     //     _busAccess.busSocketEnable(_busSocketId, false);

//     //     // Release bus hold
//     //     _busAccess.waitHold(_busSocketId, false);

//     //     // Remove paging for injection
//     //     _busAccess.targetPageForInjection(_busSocketId, false);
//     //     _pageOutForInjectionActive = false;

//     //     return true;
//     // }
//     return false;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle bus actions
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::busActionCompleteStatic(void* pObject, BR_BUS_ACTION actionType,  BR_BUS_ACTION_REASON reason)
// {
//     if(!pObject)
//         return;
//     ((TargetControl*)pObject)->busActionComplete(actionType, reason);
// }

// void TargetControl::busActionComplete(BR_BUS_ACTION actionType,  BR_BUS_ACTION_REASON reason)
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG,"bus action complete type %d reason %d programPending %d", 
//     //         actionType, reason, _busActionPendingProgramTarget);

//     if (actionType == BR_BUS_ACTION_RESET)
//     {
//         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionComplete Reset");
//         _prefixTracker[0] = _prefixTracker[1] = false;

//         // Since we're receiving a reset we are at the start of the program so clear prefix-tracking
//         _targetStateAcqMode = TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR;

//         // Check for reset pending (the reset may have been caused by another module so only start injecting if
//         // we asked for it)
//         if (_targetResetPending)
//         {
//             LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionComplete Reset pending - go straight to inject");
//             // Go straight into injecting mode
//             _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;
//             _targetResetPending = false;
//         }
//     }
//     else if (actionType == BR_BUS_ACTION_RESET_END)
//     {
//         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionComplete ResetEnd");
//     }
//     else if (actionType == BR_BUS_ACTION_BUSRQ)
//     {
//         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionComplete BUSRQ");

//         // Program target pending?
//         if (_busActionPendingProgramTarget)
//         {
//             LogWrite(MODULE_PREFIX, LOG_NOTICE, "busActionComplete pendingProgramTarget numBlocks %d",
//                             _targetProgrammer.numMemoryBlocks());

//             // Write the blocks

//             // TODO 2020

//             // // TODO 2020 - using _busaccess instead of hw for blockwrite
//             // _busActionCodeWrittenAtResetVector = false;
//             // for (int i = 0; i < _targetProgrammer.numMemoryBlocks(); i++) {
//             //     TargetProgrammer::TargetMemoryBlock* pBlock = _targetProgrammer.getMemoryBlock(i);
//             //     BR_RETURN_TYPE brResult = _busAccess.blockWrite(pBlock->start, 
//             //                 _targetProgrammer.getMemoryImagePtr() + pBlock->start, pBlock->len,
//             //                 BLOCK_ACCESS_MEM);
//             //     LogWrite(MODULE_PREFIX, LOG_DEBUG,"ProgramTarget done %08x len %d result %d micros %u", pBlock->start, pBlock->len, brResult, micros());
//             //     if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
//             //         _busActionCodeWrittenAtResetVector = true;
//             // }

//             // // Debug
//             // uint8_t testBlock[0x100];
//             // uint32_t baseAddr = 0x6000;
//             // uint32_t blockLen = 0x100;
//             // BusAccess::blockRead(baseAddr, testBlock, blockLen, false, false);
//             // char buf2[100];
//             // buf2[0] = 0;
//             // uint32_t lineStart = 0;
//             // for (uint32_t i = 0; i < blockLen; i++)
//             // {
//             //     char buf1[10];
//             //     ee_sprintf(buf1, "%02x ", testBlock[i]);
//             //     strlcat(buf2, buf1, 100);
//             //     if (i % 0x10 == 0x0f)
//             //     {
//             //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "%04x %s", baseAddr+lineStart, buf2);
//             //         buf2[0] = 0;
//             //         lineStart = i+1;
//             //     }
//             // }

//             // Check for exec
//             if (_busActionPendingExecAfterProgram)
//                 targetExec();

//             // No longer pending
//             _busActionPendingProgramTarget = false;
//         }
//     }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle wait callbacks
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     if (!pObject)
//         return;
//     ((TargetControl*)pObject)->handleWaitInterrupt(addr, data, flags, retVal);
// }

// void TargetControl::handleWaitInterrupt(uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     // Only handle MREQs
//     if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
//         return;

// //    LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAITISRSTART %s %s A %04x D %02x M1 %d Pfx0 %d Pfx1 %d Flags %08x", 
// //                 (_stepMode == STEP_MODE_STEP_PAUSED) ? "PAUSED" : ((_stepMode == STEP_MODE_STEP_INTO) ? "INTO" : ((_stepMode == STEP_MODE_STEP_OVER) ? "OVER" : "RUN")),
// //                 (_targetStateAcqMode == TARGET_STATE_ACQ_NONE) ? "ACQNONE" : ((_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING) ? "INJECTING" : ((_targetStateAcqMode == TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR) ? "INJIFNEW" : "POSTINJ")),
// //                 addr, 
// //                 data, 
// //                 (flags & BR_CTRL_BUS_M1_MASK) != 0, _prefixTracker[0], _prefixTracker[1], 
// //                 flags);

//     // TODO 2020 - RESTORE
//     // // Handle state machine
//     // TARGET_STATE_ACQ startAcqMode = _targetStateAcqMode;
//     // switch (_targetStateAcqMode)
//     // {
//     //     case TARGET_STATE_ACQ_NONE:
//     //     case TARGET_STATE_ACQ_POST_INJECT:
//     //     {
//     //         // Check for post inject hold
//     //         if ((_targetStateAcqMode == TARGET_STATE_ACQ_POST_INJECT) && (_stepMode == STEP_MODE_STEP_PAUSED))
//     //         {
//     //             // Tell bus to hold at this point
//     //             // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitISR postInject && paused -> hold = true");
//     //             _busAccess.waitHold(_busSocketId, true);
//     //         }

//     //         // Check for step-over, breakpoints, etc
//     //         handleStepOverBkpts(addr, data, flags, retVal);

//     //         // Look for start of instruction
//     //         handleTrackerIdle(addr, data, flags, retVal);

//     //         break;
//     //     }
//     //     case TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR:
//     //     {
//     //         // TODO 2020
//     //         // RE-implement heartbeat while debugging

//     //         // // Machine heartbeat handler
//     //         // if (flags & BR_CTRL_BUS_M1_MASK)
//     //         // {
//     //         //     _machineHeartbeatCounter++;
//     //         //     if (_machineHeartbeatCounter > 1000)
//     //         //     {
//     //         //         _mcManager.machineHeartbeat();
//     //         //         _machineHeartbeatCounter = 0;
//     //         //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "INT");
//     //         //     }
//     //         // }

//     //         // Check for disable
//     //         if (handlePendingDisable())
//     //             break;

//     //         // Check for step-over, breakpoints, etc
//     //         handleStepOverBkpts(addr, data, flags, retVal);

//     //         // // Remove any hold to allow execution / injection
//     //         // busAccess.waitHold(_busSocketId, false);

//     //         // If we get another first byte then start injecting
//     //         bool firstByteOfInstr = trackPrefixedInstructions(flags, data, retVal);
//     //         if (!firstByteOfInstr)
//     //         {
//     //             // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAITFIRST %04x %02x %s%s %d %d", 
//     //             //         addr, data, 
//     //             //         (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", 
//     //             //         (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "",
//     //             //         _prefixTracker[0], _prefixTracker[1]);
//     //             // Debug
//     //             if (_debugInstrBytePos < MAX_BYTES_IN_INSTR)
//     //                 _debugInstrBytes[_debugInstrBytePos++] = data;
//     //         }
//     //         else
//     //         {
//     //             // LogWrite(MODULE_PREFIX, LOG_DEBUG, "GOTFIRST %04x %02x %s%s %d %d", 
//     //             //         addr, data, 
//     //             //         (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", 
//     //             //         (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "",
//     //             //         _prefixTracker[0], _prefixTracker[1]);
//     //             // Bump state if in step mode or a grab is needed
//     //             if ((_stepMode == STEP_MODE_STEP_INTO) || _requestDisplayWhileStepping)
//     //             {
//     //                 // TODO DEBUG
//     //                 // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_A);
//     //                 _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;
//     //             }
//     //         }

//     //         // Check for move to injection state and start now
//     //         if (_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING)
//     //         {
//     //             _setRegs = false;
//     //             _snippetPos = 0;
//     //             handleInjection(addr, data, flags, retVal);
//     //         }

//     //         break;
//     //     }
//     //     case TARGET_STATE_ACQ_INJECTING:
//     //     {
//     //         // Perform injection
//     //         handleInjection(addr, data, flags, retVal);
//     //         break;
//     //     }
//     // }

//     // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAITISRDONE %s %s RetVal %02x", 
//     // //             (_stepMode == STEP_MODE_STEP_PAUSED) ? "PAUSED" : ((_stepMode == STEP_MODE_STEP_INTO) ? "INTO" : ((_stepMode == STEP_MODE_STEP_OVER) ? "OVER" : "RUN")),
//     // //             (_targetStateAcqMode == TARGET_STATE_ACQ_NONE) ? "ACQNONE" : ((_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING) ? "INJECTING" : ((_targetStateAcqMode == TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR) ? "INJIFNEW" : "POSTINJ")),
//     // //             retVal);

//     // if (!((_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING) && (startAcqMode == TARGET_STATE_ACQ_INJECTING)))
//     // {
//     //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "addr %04x RetVal %02x(%c) Held %c InData %02x flags %08x %c%c%c%c%c Pfx1 %d Pfx2 %d state %d startState %d", 
//     //     //             addr, retVal & 0xff, 
//     //     //             (retVal & BR_MEM_ACCESS_INSTR_INJECT) ? 'I' : ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? 'X' : 'D'),
//     //     //             busAccess.waitIsHeld() ? 'Y' : 'N',
//     //     //             data,
//     //     //             flags, 
//     //     //             (flags & BR_CTRL_BUS_M1_MASK) ? '1' : ' ', 
//     //     //             (flags & BR_CTRL_BUS_WR_MASK) ? 'W' : ' ', 
//     //     //             (flags & BR_CTRL_BUS_RD_MASK) ? 'R' : ' ', 
//     //     //             (flags & BR_CTRL_BUS_MREQ_MASK) ? 'M' : ' ', 
//     //     //             (flags & BR_CTRL_BUS_IORQ_MASK) ? 'I' : ' ', 
//     //     //             _prefixTracker[0], _prefixTracker[1],
//     //     //             _targetStateAcqMode, startAcqMode);
//     // }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle stepping over a breakpoint
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::handleStepOverBkpts( uint32_t addr,  uint32_t data, 
//         uint32_t flags,  uint32_t& retVal)
// {
//     // Ignore if injecting
//     if ((_targetStateAcqMode == TARGET_STATE_ACQ_INJECTING) || 
//                 (_targetStateAcqMode == TARGET_STATE_ACQ_POST_INJECT))
//         return;

//     // Step-over handling: enabled and M1 cycle
//     if ((_stepMode == STEP_MODE_STEP_OVER) && (flags & BR_CTRL_BUS_M1_MASK) && (_stepOverPCValue == addr))
//     {
//         _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;
//         _stepMode = STEP_MODE_STEP_PAUSED;
//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hit step-over PC value %04x", _stepOverPCValue);
//     }
//     else if (_breakpoints.checkForBreak(addr, data, flags, retVal))
//     {
//         _targetStateAcqMode = TARGET_STATE_ACQ_INJECTING;
//         _stepMode = STEP_MODE_STEP_PAUSED;
//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hit Breakpoint %04x", addr);
//     }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle tracking when idle
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::handleTrackerIdle( uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     // Check for disable
//     if (handlePendingDisable())
//         return;

//     // If we detect the first byte of an instruction then move state
//     bool firstByteOfInstr = trackPrefixedInstructions(flags, data, retVal);
//     if (firstByteOfInstr)
//         _targetStateAcqMode = TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR;
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "NONE %s %04x %s%s %d %d", 
//     //                 firstByteOfInstr ? "FIRSTNONPFX" : "",
//     //                 addr, 
//     //                 (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "",
//     //                 _prefixTracker[0], _prefixTracker[1]);

//     // Debug
//     if (firstByteOfInstr || (_debugInstrBytePos >= MAX_BYTES_IN_INSTR))
//         _debugInstrBytePos = 0;
//     _debugInstrBytes[_debugInstrBytePos++] = data;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Handle injection
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::handleInjection(uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     // TODO 2020
// //     // Use the bus socket to request page out if required
// //     if (!_pageOutForInjectionActive)
// //     {
// //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetPageForInjection TRUE");
// //         _busAccess.targetPageForInjection(_busSocketId, true);
// //         _pageOutForInjectionActive = true;
// //     }
    
// //     // Handle get or set
// //     OPCODE_INJECT_PROGRESS injectProgress = OPCODE_INJECT_GENERAL;
// //     if (_setRegs)
// //         injectProgress = handleRegisterSet(retVal);
// //     else
// //         injectProgress = handleRegisterGet(addr, data, flags, retVal);

// //     // Check if time for memory grab
// //     if (injectProgress == OPCODE_INJECT_GRAB_MEMORY)
// //     {
// //         // Check if post-inject memory mirroring required
// //         if (_postInjectMemoryMirror || _requestDisplayWhileStepping)
// //         {

// //             // Suspend bus detail after a BUSRQ as there is a hardware problem with FF_DATA_OE_BAR remaining
// //             // enabled after a BUSRQ which causes contention issues on the PIB. The way around this is to
// //             // ensure BUSRQ is handled synchronously with the TargetControl operation when MREQ waits are
// //             // enabled so that the cycle immediately following a BUSRQ in this case will be part of the 
// //             // opcode injection cycle and the bus detail will not be required
// //             _busAccess.waitSuspendBusDetailOneCycle();

// //             // Use the bus socket to request page-in
// //             _busAccess.targetPageForInjection(_busSocketId, false);
// //             _pageOutForInjectionActive = false;

// //             // Request bus
// //             // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Request bus for mirror");
// //             BR_BUS_ACTION_REASON busAction = BR_BUS_ACTION_MIRROR;
// //             if (_requestDisplayWhileStepping)
// //                 busAction = BR_BUS_ACTION_DISPLAY;
// //             _requestDisplayWhileStepping = false;
// //             _busAccess.targetReqBus(_busSocketId, busAction);
// //             _postInjectMemoryMirror = false;
// //         }
// //     }
// //     else if (injectProgress == OPCODE_INJECT_DONE)
// //     {
// //         // Clear the prefix tracker
// //         _prefixTracker[0] = _prefixTracker[1] = false;

// //         // Default back to getting
// //         _setRegs = false;
// //         _snippetPos = 0;

// //         // Use the bus socket to request page-in delayed to next wait event
// //         _busAccess.targetPageForInjection(_busSocketId, false);
// //         _pageOutForInjectionActive = false;

// //         // Go back to allowing a single instruction to run before reg get
// // #ifdef PAUSE_GET_REGS_AT_CUR_ADDR
// //         _targetStateAcqMode = TARGET_STATE_ACQ_POST_INJECT;
// //         if (_stepMode == STEP_MODE_STEP_INTO)
// //         {
// //             _stepMode = STEP_MODE_STEP_PAUSED;
// //         }
// // #else
// //         _targetStateAcqMode = TARGET_STATE_ACQ_NONE;
// //         // Check for hold required
// //         if (_stepMode == STEP_MODE_STEP_INTO)
// //         {
// //             // Tell bus to hold at this point
// //             busAccess.waitHold(_busSocketId, true);
// //         }
// // #endif

// //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "INJECTING FINISHED 0x%04x 0x%02x %s%s",
// //         //             addr, ((flags & BR_CTRL_BUS_RD_MASK) & ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0)) ? (retVal & 0xff) : data,  
// //         //             (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

// //         // TODO
// //         // char regBuf[1000];
// //         // _z80Registers.format(regBuf, 1000);
// //         // char tmpBuf[10];
// //         // for (uint32_t i = 0; i < _debugInstrBytePos; i++)
// //         // {    
// //         //     ee_sprintf(tmpBuf, " %02x", _debugInstrBytes[i]);
// //         //     strlcat(regBuf, tmpBuf, 1000);
// //         // }
// //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "%s", regBuf);

// //         // TODO may need to call back to whoever requested this
// //     }
// //     else
// //     {
// //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "INJECTING %04x %02x %s%s",
// //         //             addr, ((flags & BR_CTRL_BUS_RD_MASK) & ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0)) ? (retVal & 0xff) : data, 
// //         //             (flags & BR_CTRL_BUS_RD_MASK) ? "R" : "", (flags & BR_CTRL_BUS_WR_MASK) ? "W" : "");

// //     }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Start target program - or reset
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::targetExec()
// {
//     // TODO 2020
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Starting target code, debugActive %c", 
//     //                 getTargetTracker().isTrackingActive() ? 'Y' : 'N');
//     bool performHardReset = true;
//     // TODO 2020
//     // if (getTargetProgrammer().areRegistersValid())
//     // {
//     //     // Check how to set registers
//     //     if (getHwManager().getOpcodeInjectEnable() || getTargetTracker().isTrackingActive())
//     //     {
//     //         // Use the TargetControl module to inject instructions to set registers
//     //         Z80Registers regs;
//     //         getTargetProgrammer().getTargetRegs(regs);
//     //         getTargetTracker().startSetRegisterSequence(&regs);
//     //         performHardReset = false;
//     //     }
//     //     else
//     //     {
//     //         // If the code doesn't start at 0 or a start location has been supplied,
//     //         // generate a code snippet to set registers and run
//     //         if (!_busActionCodeWrittenAtResetVector || getTargetProgrammer().areRegistersValid())
//     //         {
//     //             uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
//     //             Z80Registers regs;
//     //             getTargetProgrammer().getTargetRegs(regs);
//     //             static const int REGISTERS_STR_MAX_LEN = 500;
//     //             char regsStr[REGISTERS_STR_MAX_LEN];
//     //             regs.format(regsStr, REGISTERS_STR_MAX_LEN);
//     //             LogWrite(MODULE_PREFIX, LOG_DEBUG, "Regs: %s", regsStr);
//     //             uint32_t codeDestAddr = 0;
//     //             if (_pCurMachine)
//     //                 _pCurMachine->getSetRegistersCodeAddr();
//     //             int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
//     //             if (codeLen != 0)
//     //             {
//     //                 // Reg setting code
//     //                 LogWrite(MODULE_PREFIX, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
//     //                 getHwManager().blockWrite(codeDestAddr, regSetCode, codeLen, false, false, false);
                    
//     //                 // Reset vector
//     //                 uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
//     //                 getHwManager().blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, false, false, false);

//     //                 // TODO 2020
//     //                 // LogDumpMemory(MODULE_PREFIX, LOG_DEBUG, regSetCode, regSetCode + codeLen);
//     //             }
//     //         }
//     //     }
//     // }

//     // // See if we need to do a hard reset
//     // if (performHardReset)
//     // {
//     //     // Request reset target
//     //     _busAccess.targetReqReset(_busSocketId);
//     // }
// }
