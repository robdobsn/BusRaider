// Bus Raider
// Rob Dobson 2019-2022

#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "TargetProgrammer.h"
#include "MemoryInterface.h"
#include "CPUHandler.h"
#include "HwManager.h"
#include "CPUHandler_Z80.h"

#define DEBUG_TARGET_PROGRAMMING

// Module name
static const char MODULE_PREFIX[] = "TargProg";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TargetProgrammer::TargetProgrammer(MemoryInterface& memoryInterface,
        CPUHandler& cpuHandler, HwManager& hwManager,
        TargetImager& targetImager)
    : _memoryInterface(memoryInterface),
      _cpuHandler(cpuHandler),
      _hwManager(hwManager),
      _targetImager(targetImager)
{
}

void TargetProgrammer::clear()
{
    _programmingStartPending = false;
    _programmingDoExec = false;
    _programmingEnterDebugger = false;
}

void TargetProgrammer::programmingStart(bool execAfterProgramming, bool enterDebugger)
{
    // Check there is something to write
    if (_targetImager.numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart - nothing to write");
    }

    // Indicate that programming is pending
#ifdef DEBUG_TARGET_PROGRAMMING
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart targetReqBus");
#endif
    _programmingStartPending = true;
    _programmingDoExec = execAfterProgramming;
    _programmingEnterDebugger = enterDebugger;
}

void TargetProgrammer::programmingWrite()
{
    // Program
#ifdef DEBUG_TARGET_PROGRAMMING
    LogWrite(MODULE_PREFIX, LOG_NOTICE, "programmingWrite pending %d numBlocks %d",
                    _programmingStartPending, _targetImager.numMemoryBlocks());
#endif

    // Write the blocks
    bool codeAtResetVector = false;
    for (uint32_t i = 0; i < _targetImager.numMemoryBlocks(); i++) 
    {
        TargetImager::TargetMemoryBlock* pBlock = _targetImager.getMemoryBlock(i);
#ifdef DEBUG_TARGET_PROGRAMMING
        BR_RETURN_TYPE brResult = 
#endif
        _memoryInterface.blockWrite(pBlock->start, 
                    _targetImager.getMemoryImagePtr() + pBlock->start, pBlock->len,
                    BLOCK_ACCESS_MEM);
#ifdef DEBUG_TARGET_PROGRAMMING
        LogWrite(MODULE_PREFIX, LOG_DEBUG,
                            "imagerWrite done %08x len %d result %d micros %u", 
                            pBlock->start, pBlock->len, brResult, micros());
#endif
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
    else if (_programmingEnterDebugger)
        _cpuHandler.debuggerBreak();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start target program
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetProgrammer::programExec(bool codeAtResetVector)
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "programExec codeAtResetVector %c",
            codeAtResetVector ? 'Y' : 'N');

    bool performHardReset = true;
    if (_targetImager.areRegistersValid())
    {
        // TODO 2020
        // // Check how to set registers
        // if (getHwManager().getOpcodeInjectEnable() || getTargetTracker().isTrackingActive())
        // {
        //     // Use the CPUHandler module to inject instructions to set registers
        //     Z80Registers regs;
        //     getTargetProgrammer().getTargetRegs(regs);
        //     getTargetTracker().startSetRegisterSequence(&regs);
        //     performHardReset = false;
        // }
        // else
        // {
            // If the code doesn't start at 0 or a start location has been supplied,
            // generate a code snippet to set registers and run
            if (!codeAtResetVector || _targetImager.areRegistersValid())
            {
                // Get registers and code address to set
                uint8_t executorCode[MAX_EXECUTOR_CODE_LEN];
                Z80Registers regs;
                _targetImager.getTargetRegs(regs);
                static const int REGISTERS_STR_MAX_LEN = 500;
                char regsStr[REGISTERS_STR_MAX_LEN];
                regs.format(regsStr, REGISTERS_STR_MAX_LEN);
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Regs: %s", regsStr);
                uint32_t codeDestAddr = _targetImager.getSetRegistersCodeAddr();

                // Code for hardware setup
                uint32_t hwCodeLen = _hwManager.getSnippetToSetupHw(codeDestAddr, executorCode, MAX_EXECUTOR_CODE_LEN);

                // Code to set regs
                uint32_t regsCodeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr+hwCodeLen, regs, executorCode+hwCodeLen, 
                                    MAX_EXECUTOR_CODE_LEN-hwCodeLen);
                uint32_t totalCodeLen = hwCodeLen + regsCodeLen;
                if (totalCodeLen != 0)
                {
                    // Executor code
#ifdef DEBUG_TARGET_PROGRAMMING
                    LogWrite(MODULE_PREFIX, LOG_DEBUG,"Set hw & regs snippet at %04x len %d starts %02x %02x %02x %02x", 
                                codeDestAddr, totalCodeLen, executorCode[0], executorCode[1], executorCode[2], executorCode[3]);
#endif
                    _memoryInterface.blockWrite(codeDestAddr, executorCode, totalCodeLen, BLOCK_ACCESS_MEM);

                    // Reset vector
                    uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
                    _memoryInterface.blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, BLOCK_ACCESS_MEM);
                    // TODO 2020
                    // LogDumpMemory(MODULE_PREFIX, LOG_DEBUG, executorCode, executorCode + totalCodeLen);
                }
            }
        // }
    }

    // See if we need to do a hard reset
    if (performHardReset)
    {
        // Enter debugger if required
        if (_programmingEnterDebugger)
            _cpuHandler.debuggerBreak();

        // Request reset target
        _cpuHandler.targetReset();
    }
}
