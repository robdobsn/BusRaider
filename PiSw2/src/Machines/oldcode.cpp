
    // LogWrite(MODULE_PREFIX, LOG_DEBUG,"Bus action active type %d reason %d programPending %d dispPending %d", 
    //         actionType, reason, _busActionPendingProgramTarget, _busActionPendingDisplayRefresh);
    // We don't care what the reason for the BUSRQ is we will use it for what we need
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionActiveStatic BUSRQ");

        // Program target pending?
        if (_busActionPendingProgramTarget)
        {
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionActiveStatic pendingProgramTarget numBlocks %d",
            //                 getTargetProgrammer().numMemoryBlocks());

            // Write the blocks
            _busActionCodeWrittenAtResetVector = false;
            for (int i = 0; i < getTargetProgrammer().numMemoryBlocks(); i++) {
                TargetProgrammer::TargetMemoryBlock* pBlock = getTargetProgrammer().getMemoryBlock(i);
                BR_RETURN_TYPE brResult = getHwManager().blockWrite(pBlock->start, 
                            getTargetProgrammer().getMemoryImagePtr() + pBlock->start, pBlock->len, false, false, false);
                LogWrite(MODULE_PREFIX, LOG_DEBUG,"ProgramTarget done %08x len %d result %d micros %u", pBlock->start, pBlock->len, brResult, micros());
                if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
                    _busActionCodeWrittenAtResetVector = true;
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
            if (_busActionPendingExecAfterProgram)
                targetExec();

            // No longer pending
            _busActionPendingProgramTarget = false;
        }

        // Display refresh pending?
        if (_busActionPendingDisplayRefresh)
        {
            // Call the machine to handle
            if (_pCurMachine)
                _pCurMachine->refreshDisplay();
            _busActionPendingDisplayRefresh = false;    
        }
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Handle control of target machine
void McManager::targetReset()
{
    // Reset target
    _busControl.targetReqReset(_busSocketId);
}

// TODO 2020 - move to BusAccess
void McManager::targetIrq(int durationTStates)
{
    // Generate a maskable interrupt
    _busControl.targetReqIRQ(_busSocketId, durationTStates);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::targetProgrammingStart(bool execAfterProgramming)
{
    // Check there is something to write
    if (getTargetProgrammer().numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "ProgramTarget - nothing to write");
    } 
    else 
    {
        // BUSRQ is used even if memory is emulated because it holds the processor while changes are made
        // Give the BusAccess some service first to ensure WAIT handling is complete before requesting the bus
        for (int i = 0; i < 3; i++)
            _busControl.service();

        // Request target bus
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetProgStart targetReqBus");
        _busControl.targetReqBus(_busSocketId, BR_BUS_ACTION_PROGRAMMING);
        _busActionPendingProgramTarget = true;
        _busActionPendingExecAfterProgram = execAfterProgramming;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start target program - or reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetExec()
{
    // TODO 2020
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Starting target code, debugActive %c", 
    //                 getTargetTracker().isTrackingActive() ? 'Y' : 'N');
    bool performHardReset = true;
    // TODO 2020
    // if (getTargetProgrammer().areRegistersValid())
    // {
    //     // Check how to set registers
    //     if (getHwManager().getOpcodeInjectEnable() || getTargetTracker().isTrackingActive())
    //     {
    //         // Use the TargetTracker module to inject instructions to set registers
    //         Z80Registers regs;
    //         getTargetProgrammer().getTargetRegs(regs);
    //         getTargetTracker().startSetRegisterSequence(&regs);
    //         performHardReset = false;
    //     }
    //     else
    //     {
    //         // If the code doesn't start at 0 or a start location has been supplied,
    //         // generate a code snippet to set registers and run
    //         if (!_busActionCodeWrittenAtResetVector || getTargetProgrammer().areRegistersValid())
    //         {
    //             uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
    //             Z80Registers regs;
    //             getTargetProgrammer().getTargetRegs(regs);
    //             static const int REGISTERS_STR_MAX_LEN = 500;
    //             char regsStr[REGISTERS_STR_MAX_LEN];
    //             regs.format(regsStr, REGISTERS_STR_MAX_LEN);
    //             LogWrite(MODULE_PREFIX, LOG_DEBUG, "Regs: %s", regsStr);
    //             uint32_t codeDestAddr = 0;
    //             if (_pCurMachine)
    //                 _pCurMachine->getSetRegistersCodeAddr();
    //             int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
    //             if (codeLen != 0)
    //             {
    //                 // Reg setting code
    //                 LogWrite(MODULE_PREFIX, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
    //                 getHwManager().blockWrite(codeDestAddr, regSetCode, codeLen, false, false, false);
                    
    //                 // Reset vector
    //                 uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
    //                 getHwManager().blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, false, false, false);

    //                 // TODO 2020
    //                 // LogDumpMemory(MODULE_PREFIX, LOG_DEBUG, regSetCode, regSetCode + codeLen);
    //             }
    //         }
    //     }
    // }

    // See if we need to do a hard reset
    if (performHardReset)
    {
        // Request reset target
        _busControl.targetReqReset(_busSocketId);
    }
}





    // Pending actions
    bool _busActionPendingProgramTarget;
    bool _busActionPendingExecAfterProgram;


	// Hardware manager
	HwManager m_HwManager;


	m_HwManager(m_CommsManager.getCommandHandler(), m_busControl),

