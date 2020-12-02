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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugger grab registers and memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::debuggerGrabRegsAndMemory(uint32_t addr, uint32_t data, uint32_t flags)
{
        // DEBUG_PULSE();


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

    // Check if this is an instruction fetch cycle
    if (!((flags & BR_CTRL_BUS_M1_MASK) && (flags & BR_CTRL_BUS_MREQ_MASK)))
        return;

    // // Iterate through registers
    // for (uint32_t regInstr = 0; regInstr < )
    // // Check if writing
    // if (flags & BR_CTRL_BUS_WR_MASK)
    // {
    //     retVal = BR_MEM_ACCESS_INSTR_INJECT;
    //     // Store the result in registers
    //     switch(_snippetPos)
    //     {
    //         case 1:  // push af
    //         {
    //             if (_snippetWriteIdx == 0)
    //             {
    //                 _z80Registers.SP = addr+1;
    //                 _z80Registers.AF = data << 8;
    //                 regQueryInstructions[RegisterAFUpdatePos+1] = data;                     
    //                 _snippetWriteIdx++;
    //             }
    //             else
    //             {
    //                 _z80Registers.AF = (_z80Registers.AF & 0xff00) | (data & 0xff);
    //                 regQueryInstructions[RegisterAFUpdatePos] = data;                     
    //             }
    //             break;
    //         }
    //         case 4: // ld (hl), a (where a has the contents of r)
    //         {
    //             _z80Registers.HL = addr;
    //             // R value compensates for the push af and ld r,a instructions
    //             _z80Registers.R = data - 3;
    //             // Value stored back to R compensates for ld a,NN and jr loop instructions
    //             regQueryInstructions[RegisterRUpdatePos] = _z80Registers.R - 2;                 
    //             break;
    //         }
    //         case 7: // ld (hl), a (where a has the contents of i)
    //         {
    //             _z80Registers.I = data;                        
    //             break;
    //         }
    //         case 10: // ld (de), a
    //         {
    //             _z80Registers.DE = addr;                        
    //             break;
    //         }
    //         case 11: // ld (bc), a
    //         {
    //             _z80Registers.BC = addr;                        
    //             break;
    //         }
    //         case 13: // ld (hl), a (after EXX so hl')
    //         {
    //             _z80Registers.HLDASH = addr;                        
    //             break;
    //         }
    //         case 14: // ld (de), a (after EXX so de')
    //         {
    //             _z80Registers.DEDASH = addr;                        
    //             break;
    //         }
    //         case 15: // ld (bc), a (after EXX so bc')
    //         {
    //             _z80Registers.BCDASH = addr;                        
    //             break;
    //         }
    //         case 18:  // push af (actually af')
    //         {
    //             if (_snippetWriteIdx == 0)
    //             {
    //                 _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff) | (data << 8);
    //                 _snippetWriteIdx++;
    //             }
    //             else
    //             {
    //                 _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff00) | (data & 0xff);
    //             }
    //             break;
    //         }
    //         case 23:  // push ix
    //         {
    //             if (_snippetWriteIdx == 0)
    //             {
    //                 _z80Registers.IX = (_z80Registers.IX & 0xff) | (data << 8);
    //                 _snippetWriteIdx++;
    //             }
    //             else
    //             {
    //                 _z80Registers.IX = (_z80Registers.IX & 0xff00) | (data & 0xff);
    //             }
    //             break;
    //         }
    //         case 27:  // push ix
    //         {
    //             if (_snippetWriteIdx == 0)
    //             {
    //                 _z80Registers.IY = (_z80Registers.IY & 0xff) | (data << 8);
    //                 _snippetWriteIdx++;
    //             }
    //             else
    //             {
    //                 _z80Registers.IY = (_z80Registers.IY & 0xff00) | (data & 0xff);
    //             }
    //             break;
    //         }
    //     }
    // }
    // else
    // {
    //     if (_snippetPos == 0)
    //         _z80Registers.PC = addr;
    //     // Send instructions to query registers
    //     retVal = regQueryInstructions[_snippetPos++] | BR_MEM_ACCESS_INSTR_INJECT;
    //     _snippetWriteIdx = 0;
    // }

    // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "regGet pos %d D %02x flags %c %c %c %c %c %02x", 
    // //             _snippetPos-1,
    // //             retVal,
    // //             (flags & BR_CTRL_BUS_M1_MASK) ? '1' : ' ',
    // //             (flags & BR_CTRL_BUS_RD_MASK) ? 'R' : ' ',
    // //             (flags & BR_CTRL_BUS_WR_MASK) ? 'W' : ' ',
    // //             (flags & BR_CTRL_BUS_MREQ_MASK) ? 'M' : ' ',
    // //             (flags & BR_CTRL_BUS_IORQ_MASK) ? 'I' : ' ',
    // //             flags);

    // // Check if complete
    // if (_snippetPos >= sizeof(regQueryInstructions))
    // {
    //     _snippetPos = 0;
    //     // char regStr[200];
    //     // _z80Registers.format(regStr, 200);
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Reg values: %s", regStr);
    //     return OPCODE_INJECT_DONE;
    // }
    // else if (_snippetPos == RelJumpBackStartPos)
    // {
    //     return OPCODE_INJECT_GRAB_MEMORY;
    // }
    // return OPCODE_INJECT_GENERAL;
}
