// Bus Raider
// Rob Dobson 2019

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "TargetCPUZ80.h"

// Module name
static const char MODULE_PREFIX[] = "TargCtrlProg";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::programmingClear()
{
    _programmingPending = false;
    _programmingDoExec = false;
}

void TargetControl::programmingStart(bool execAfterProgramming)
{
    // Check there is something to write
    if (_targetProgrammer.numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart - nothing to write");
    } 
    else 
    {
        // Indicate that programming is pending
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart targetReqBus");
        _programmingPending = true;
        _programmingDoExec = execAfterProgramming;

        // // BUSRQ is used even if memory is emulated because it holds the processor while changes are made
        // // Give the BusAccess some service first to ensure WAIT handling is complete before requesting the bus
        // for (int i = 0; i < 3; i++)
        //     _busControl.service();

        // // Request target bus
        // _busControl.socketReqBus(_busSocketId, BR_BUS_ACTION_PROGRAMMING);
        // _busActionPendingProgramTarget = true;
        // _busActionPendingExecAfterProgram = execAfterProgramming;
    }
}

void TargetControl::programmingWrite()
{
    // Program
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "programmingWrite pending %d numBlocks %d",
    //                 _programmingPending, _targetProgrammer.numMemoryBlocks());

    // Write the blocks
    bool codeAtResetVector = false;
    for (uint32_t i = 0; i < _targetProgrammer.numMemoryBlocks(); i++) 
    {
        TargetProgrammer::TargetMemoryBlock* pBlock = _targetProgrammer.getMemoryBlock(i);
        // BR_RETURN_TYPE brResult = 
        _busControl.mem().blockWrite(pBlock->start, 
                    _targetProgrammer.getMemoryImagePtr() + pBlock->start, pBlock->len,
                    BLOCK_ACCESS_MEM);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG,
        //                     "ProgramTarget done %08x len %d result %d micros %u", 
        //                     pBlock->start, pBlock->len, brResult, micros());
        if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
            codeAtResetVector = true;
    }

    // // Debug
    // uint8_t testBlock[0x100];
    // uint32_t baseAddr = 0x6000;
    // uint32_t blockLen = 0x100;
    // BusAccess::blockRead(baseAddr, testBlock, blockLen, false, false);
    // char buf2[100];
    // buf2[0] = 0;
    // uint32_t lineStart = 0;
    // for (uint32_t i = 0; i < blockLen; i++)
    // {
    //     char buf1[10];
    //     ee_sprintf(buf1, "%02x ", testBlock[i]);
    //     strlcat(buf2, buf1, 100);
    //     if (i % 0x10 == 0x0f)
    //     {
    //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "%04x %s", baseAddr+lineStart, buf2);
    //         buf2[0] = 0;
    //         lineStart = i+1;
    //     }
    // }

    // Check for exec
    if (_programmingDoExec)
        programExec(codeAtResetVector);
}

void TargetControl::programmingDone()
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingDone");

    // No longer pending
    _programmingPending = false;

    // TODO 2020
    // Set flag to indicate mode
    // _stepMode = STEP_MODE_STEP_PAUSED;

    // // Release bus hold if held
    // if (_busControl.waitIsHeld())
    // {
    //     // Remove any hold to allow execution / injection
    //     _busControl.waitHold(_busSocketId, false);
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start target program
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::programExec(bool codeAtResetVector)
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "programExec codeAtResetVector %c",
            codeAtResetVector ? 'Y' : 'N');

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "programExec debugActive %c", 
    //                 getTargetTracker().isTrackingActive() ? 'Y' : 'N');
    bool performHardReset = true;
    if (_targetProgrammer.areRegistersValid())
    {
        // TODO 2020
        // // Check how to set registers
        // if (getHwManager().getOpcodeInjectEnable() || getTargetTracker().isTrackingActive())
        // {
        //     // Use the TargetControl module to inject instructions to set registers
        //     Z80Registers regs;
        //     getTargetProgrammer().getTargetRegs(regs);
        //     getTargetTracker().startSetRegisterSequence(&regs);
        //     performHardReset = false;
        // }
        // else
        // {
            // If the code doesn't start at 0 or a start location has been supplied,
            // generate a code snippet to set registers and run
            if (!codeAtResetVector || _targetProgrammer.areRegistersValid())
            {
                uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
                Z80Registers regs;
                _targetProgrammer.getTargetRegs(regs);
                static const int REGISTERS_STR_MAX_LEN = 500;
                char regsStr[REGISTERS_STR_MAX_LEN];
                regs.format(regsStr, REGISTERS_STR_MAX_LEN);
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Regs: %s", regsStr);
                uint32_t codeDestAddr = _targetProgrammer.getSetRegistersCodeAddr();
                int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
                if (codeLen != 0)
                {
                    // Reg setting code
                    LogWrite(MODULE_PREFIX, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
                    _busControl.mem().blockWrite(codeDestAddr, regSetCode, codeLen, BLOCK_ACCESS_MEM);

                    // Reset vector
                    uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
                    _busControl.mem().blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, BLOCK_ACCESS_MEM);
                    // TODO 2020
                    // LogDumpMemory(MODULE_PREFIX, LOG_DEBUG, regSetCode, regSetCode + codeLen);
                }
            }
        // }
    }

    // See if we need to do a hard reset
    if (performHardReset)
    {
        // Request reset target
        _busControl.bus().targetReset(_busControl.busSettings().resetDurationMs);
    }
}
