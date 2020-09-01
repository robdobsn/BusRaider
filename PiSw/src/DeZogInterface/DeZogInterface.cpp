// Bus Raider
// Rob Dobson 2019

#include "DeZogInterface.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "../System/rdutils.h"
#include "../System/lowlib.h"
#include "../System/Utils.h"
#include "../System/PiWiring.h"
#include "../TargetBus/TargetTracker.h"
#include "../TargetBus/TargetProgrammer.h"
#include "../Hardware/HwManager.h"
#include "../Machines/McManager.h"
#include "../Disassembler/src/mdZ80.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "DeZogInterface";

// This instance
DeZogInterface* DeZogInterface::_pThisInstance = NULL;

// Program version (declared in main.cpp)
extern const char* PROG_VERSION;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
DeZogInterface::DeZogInterface(CommandHandler& commandHandler, McManager& mcManager) :
        _commandHandler(commandHandler), _mcManager(mcManager)
{
    _pThisInstance = this;
    _smartloadInProgress = false;
    _smartloadStartDetected = false;
    _smartloadMsgIdx = 0;
    _smartloadStartUs = 0;
    _resetPending = 0;
    _resetPendingTimeUs = 0;
    _stepCompletionPending = false;
    _rxCmdBufferPos = 0;
    _rxCmdExpectedMsgLen = 0;
    _respBufferPos = 0;
    _regsInjectRequired = false;
}

void DeZogInterface::init()
{
    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = _commandHandler.commsSocketAdd(this, true, DeZogInterface::handleRxMsgStatic, NULL, NULL);
}

void DeZogInterface::service()
{
    // Check for smartload completion and timeouts
    // char respMsg[DEZOG_RESP_MAX_LEN];
    if (_smartloadInProgress)
    {
        if (_smartloadStartDetected)
        {
            if (!_commandHandler.isFileTransferInProgress())
            {
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Smartload completed");
                _smartloadInProgress = false;
                // strlcpy(respMsg, "Smartload OK", DEZOG_RESP_MAX_LEN);
                // addPromptMsg(respMsg, DEZOG_RESP_MAX_LEN);
                // _commandHandler.sendWithJSON("dezog", "", _smartloadMsgIdx, 
                //                 (const uint8_t*)respMsg, strlen(respMsg));

                // // Start programming process
                // McManager::targetProgrammingStart(true);

                // // Step to initiate programming and reset
                // TargetTracker::completeTargetProgram();
            }
            else if (isTimeout(micros(), _smartloadStartUs, MAX_SMART_LOAD_TIME_US))
            {
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Smartload timed-out");
                _smartloadInProgress = false;
                // strlcpy(respMsg, "Smartload failed to complete in time", DEZOG_RESP_MAX_LEN);
                // addPromptMsg(respMsg, DEZOG_RESP_MAX_LEN);
                // _commandHandler.sendWithJSON("dezog", "", _smartloadMsgIdx, 
                //             (const uint8_t*)respMsg, strlen(respMsg));
            }
        }
        else
        {
            if (_commandHandler.isFileTransferInProgress())
            {
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Smartload start detected");
                _smartloadStartDetected = true;
            }
            else if (isTimeout(micros(), _smartloadStartUs, MAX_TIME_BEFORE_START_DETECT_US))
            {
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Smartload start not detected");
                _smartloadInProgress = false;
                // strlcpy(respMsg, "Smartload start not detected", DEZOG_RESP_MAX_LEN);
                // addPromptMsg(respMsg, DEZOG_RESP_MAX_LEN);
                // _commandHandler.sendWithJSON("dezog", "", _smartloadMsgIdx, 
                //                 (const uint8_t*)respMsg, strlen(respMsg));
            }
        }
    }

    // Check for step-over complete
    if (_stepCompletionPending)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "service ...");
        // Check state of target tracker
        if (_mcManager.getTargetTracker().isStepPaused())
        {
            // Disassembly
            // uint32_t curAddr = TargetTracker::getRegs().PC;
            // uint8_t* pMirrorMemory = HwManager::getMirrorMemForAddr(0);
            // strlcpy(respMsg, "", DEZOG_RESP_MAX_LEN);
            // if (pMirrorMemory)
            // {
            //     disasmZ80(pMirrorMemory, 0, curAddr, respMsg, INTEL, false, true);
            //     mungeDisassembly(respMsg);
            // }
            // addPromptMsg(respMsg, DEZOG_RESP_MAX_LEN);

            // Form NTF_PAUSE notification message
            uint32_t msgLen = 10;
            uint32_t bufPos = 0;
            Utils::setLEUint32AndInc(_respBuffer, bufPos, msgLen);
            Utils::setUint8AndInc(_respBuffer, bufPos, 0);
            Utils::setUint8AndInc(_respBuffer, bufPos, 1);  // NTF_PAUSE
            Utils::setUint8AndInc(_respBuffer, bufPos, 0);  // Break reason
            Utils::setLEUint16AndInc(_respBuffer, bufPos, _mcManager.getTargetTracker().getRegs().PC);
            Utils::setUint8AndInc(_respBuffer, bufPos, 0);

            LogWrite(MODULE_PREFIX, LOG_DEBUG, "service StepCompletion");
            _commandHandler.sendWithJSON("dezog", "", 0, 
                            _respBuffer, msgLen);
            _stepCompletionPending = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeZogInterface::handleRxMsgStatic(void* pObject, const char* pCmdJson, const uint8_t* pParams, 
                int paramsLen, char* pRespJson, 
                int maxRespLen)
{
    if (!pObject)
        return false;
    return ((DeZogInterface*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
}

bool DeZogInterface::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, 
                [[maybe_unused]]int paramsLen, [[maybe_unused]]char* pRespJson, 
                [[maybe_unused]]int maxRespLen)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZogInterface handleRxMsg msgJson %s", pCmdJson); 

    // No direct response
    pRespJson[0] = 0;
    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 50;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;
    static const int MAX_MSGIDX_STR_LEN = 20;
    char msgIdxStr[MAX_MSGIDX_STR_LEN];
    msgIdxStr[0] = 0;
    jsonGetValueForKey("msgIdx", pCmdJson, msgIdxStr, MAX_MSGIDX_STR_LEN);
    uint32_t msgIdx = strtol(msgIdxStr, NULL, 10);

    if (strcasecmp(cmdName, "dezog") == 0)
    {
        handleDeZogCmd(pCmdJson, pParams, paramsLen, msgIdx);
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DeZog Messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void DeZogInterface::addPromptMsg(char* pResponse, int maxResponseLen)
// {
//     strlcat(pResponse, (TargetTracker::isPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);
// }

bool DeZogInterface::handleDeZogCmd([[maybe_unused]] const char* pJsonCmd, const uint8_t* pDezogFragment, 
            uint32_t dezogFragmentLen, uint32_t msgIdx)
{
    // Add to command buffer
    if (_rxCmdBufferPos + dezogFragmentLen > DEZOG_CMD_MAX_LEN)
    {
        _rxCmdBufferPos = 0;
        LogWrite(MODULE_PREFIX, LOG_WARNING, "handleRxMsg overrun rx buffer");
        return false;
    }

    // Check if this fragment contains the message length
    if (_rxCmdBufferPos == 0)
    {
        // Check length valid
        if (dezogFragmentLen < 6)
        {
            LogWrite(MODULE_PREFIX, LOG_WARNING, "handleRxMsg first msg fragment too short");
            return false;
        }

        // Extract dezog message fixed fields
        uint32_t dezogFullMsgLen = pDezogFragment[0] + pDezogFragment[1] * 256;
        _rxCmdSeqNum = pDezogFragment[4];
        _rxCmdOrResp = pDezogFragment[5];

        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog firstFrag len %d fullMsgLen %d seqNum %d cmdOrResp %d", 
        //             dezogFragmentLen, dezogFullMsgLen, _rxCmdSeqNum, _rxCmdOrResp);

        // Expected msg len
        _rxCmdExpectedMsgLen = dezogFullMsgLen;
        memcpy(_rxCmdBuffer, pDezogFragment, dezogFragmentLen);
        _rxCmdBufferPos += dezogFragmentLen;
    }
    else
    {
        // Expected msg len
        memcpy(_rxCmdBuffer+_rxCmdBufferPos, pDezogFragment, dezogFragmentLen);
        _rxCmdBufferPos += dezogFragmentLen;

        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog nextFrag len %d bufPosAfter %d fullMsgLen %d seqNum %d cmdOrResp %d", 
        //             dezogFragmentLen, _rxCmdBufferPos, _rxCmdExpectedMsgLen, _rxCmdSeqNum, _rxCmdOrResp);
    }

    // Check if full message now received
    if ((_rxCmdBufferPos > 0) && (_rxCmdBufferPos >= _rxCmdExpectedMsgLen))
    {
        // Process the message
        ProcCmdRslt procCmdRslt = processCommand(msgIdx);

        if ((procCmdRslt == PROC_CMD_FAILED) || (procCmdRslt == PROC_CMD_PARAM_ERROR))
        {
            LogWrite(MODULE_PREFIX, LOG_WARNING, "Failed decoding DeZog command 0x%02x", _rxCmdOrResp);
            _rxCmdBufferPos = 0;
            return false;
        }
        else if (procCmdRslt != PROC_CMD_NO_RESPONSE)
        {
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog respMsgLen %d rspMsgIdx %d fromResp: len %d seqIdx %d respCode %d", 
                        _respBufferPos, msgIdx, _respBuffer[0], _respBuffer[4], _respBuffer[5]);

            // Send response
            _commandHandler.sendWithJSON("dezog", "", msgIdx, _respBuffer, _respBufferPos);
        }

        // Assume fragment completed message
        _rxCmdBufferPos = 0;
        return true;
    }
    return false;
}
//     // Check valid
//     if (dezogMsgLen < 6)
//         return false;

//     // Extract dezog message
//     uint32_t dezogPayloadLen = pDezogMsg[0] + pDezogMsg[1] * 256;
//     uint32_t dezogSeqNum = pDezogMsg[4];
//     uint32_t dezogCmdOrResp = pDezogMsg[5];

//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog payloadLen %d seqNum %d cmdOrResp %d", 
//                 dezogPayloadLen, dezogSeqNum, dezogCmdOrResp);

//     // Prepare response
//     uint8_t* pRespMsg = new uint8_t[_commandHandler.MAX_DATAFRAME_LEN];
//     if (!pRespMsg)
//         return false;

//     pRespMsg[0] = 0;
//     uint32_t respMsgLen = 0;
//     bool procCmdRslt = processCommand(pDezogMsg, dezogMsgLen, dezogPayloadLen, dezogSeqNum, dezogCmdOrResp, 
//                 pRespMsg, respMsgLen, _commandHandler.MAX_DATAFRAME_LEN, dezogIndex);

//     if (!procCmdRslt)
//     {
//         LogWrite(MODULE_PREFIX, LOG_WARNING, "Failed decoding DeZog command 0x%02x", dezogCmdOrResp);
//         delete [] pRespMsg;
//         return false;
//     }

//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog respMsgLen %d dezogIdx %d len %d seqIdx %d respCode %d", 
//                 respMsgLen, dezogIndex, pRespMsg[0], pRespMsg[4], pRespMsg[5]);

//     // Send response
//     _commandHandler.sendWithJSON("dezog", "", dezogIndex, pRespMsg, respMsgLen);
//     delete [] pRespMsg;
//     return true;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Version extraction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool DeZogInterface::getSemVerFromStr(const char* inStr, int& major, int& minor, int& patch)
// {
//     // Find first digit
//     int numIdx = 0;
//     int numVal = 0;
//     bool firstDigitFound = false;
//     for (uint32_t i = 0; i < strlen(inStr); i++)
//     {
//         if (rdisdigit(inStr[i]))
//         {
//             numVal = numVal * 10 + (inStr[i] - '0');
//             firstDigitFound = true;
//         }
//         else if (firstDigitFound && (inStr[i] == '.'))
//         {
//             if (numIdx == 0)
//                 major = numVal;
//             else if (numIdx == 1)
//                 minor = numVal;
//             else
//                 patch = numVal;
//             numIdx++;
//             numVal = 0;
//         }
//     }
//     return firstDigitFound;
// }

DeZogInterface::ProcCmdRslt DeZogInterface::processCommand(uint32_t msgIdx)
{
    // Check valid
    _respBufferPos = 5;
    const uint8_t* pCmdBuf = _rxCmdBuffer + 6;
    const uint8_t* pCmdBufEnd = _rxCmdBuffer + DEZOG_CMD_MAX_LEN;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "+++++++ procCmd cmdCode %s dezogPayloadLen %d", 
                DeZogInterface::getDeZogCmdCode(_rxCmdOrResp), _rxCmdExpectedMsgLen);
    if (_rxCmdBufferPos < 64)
        Utils::logHexBuf(_rxCmdBuffer, _rxCmdBufferPos, MODULE_PREFIX, LOG_DEBUG, "DeZog Cmd");
    else
        Utils::logHexBuf(_rxCmdBuffer, 64, MODULE_PREFIX, LOG_DEBUG, "DeZog Cmd SHORTENED");

    // Response
    uint32_t respPayloadLen = _respBufferPos - 4;
    uint32_t bufPos = 0;
    Utils::setLEUint32AndInc(_respBuffer, bufPos, respPayloadLen);
    Utils::setUint8AndInc(_respBuffer, bufPos, _rxCmdSeqNum);

    switch(_rxCmdOrResp)
    {
        case CMD_INIT:
        {
            // Check buffer size ok
            if (_rxCmdBufferPos < 6)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd cmdInitTooShort");
                return PROC_CMD_PARAM_ERROR;
            }
            uint32_t programNameAndVersLenIncTerm = strlen(PROG_VERSION) + 1;
            _respBufferPos = programNameAndVersLenIncTerm + 8;
            if (DEZOG_RESP_MAX_LEN < _respBufferPos)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd responseBufTooShort");
                return PROC_CMD_PARAM_ERROR;
            }
            Utils::setUint8AndInc(_respBuffer, bufPos, 0);
            Utils::setUint8AndInc(_respBuffer, bufPos, 1);
            Utils::setUint8AndInc(_respBuffer, bufPos, 6);
            // _respBuffer[8] = 0;
            // Utils::logHexBuf(_respBuffer, _respBufferPos, MODULE_PREFIX, LOG_DEBUG, "DeZog resp partial only");
            memcpy(_respBuffer+9, PROG_VERSION, programNameAndVersLenIncTerm);

            // // Reset bus
            // BusAccess::busAccessReset();
            // _stepCompletionPending = false;

            // // Reset target
            // TargetTracker::targetReset();
            // _resetPending = true;
            // _resetPendingTimeUs = micros();

            // // Target tracker on
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargetTracker ON");
            // if (_resetPending)
            // {
            //     if (!isTimeout(micros(), _resetPendingTimeUs, MAX_TIME_RESET_PENDING_US))
            //         McManager::targetReset();
            //     _resetPending = false;
            // }
            _regsInjectRequired = false;
            _mcManager.getBusAccess().busAccessReset();

            // Enable tracker and hold in wait
            _mcManager.getTargetTracker().enable(true, true);

            break;
        }
        case CMD_GET_REGISTERS:
        {
            // Get registers
            char regStr[200];
            _mcManager.getTargetTracker().getRegsFormatted(regStr, sizeof(regStr));
            LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd getRegisters %s", regStr);
            _respBufferPos = getRegisters(_mcManager.getTargetTracker().getRegs(), 5);

            // Inject
            // if (_regsInjectRequired)
            // {
            //     LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd regsInjectRequired");
            //     _regsInjectRequired = false;
            //     McManager::targetProgrammingStart(true);
            //     TargetTracker::completeTargetProgram();
            // }
            // else
            // {
            //     LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd no regs to inject");
            // }
            break;
        }
        case CMD_SET_REGISTER:
        {
            // Check valid
            if (_rxCmdBufferPos < 8)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd cmdSetRegisterTooShort");
                return PROC_CMD_PARAM_ERROR;
            }

            // Set register
            uint32_t regNumber = _rxCmdBuffer[6];
            uint32_t valToSet = _rxCmdBuffer[7] + _rxCmdBuffer[8] * 256;
            LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd setReg regNum %d val 0x%04x", regNumber, regNumber);
            setRegister(_mcManager.getTargetTracker().getRegs(), regNumber, valToSet);
            _regsInjectRequired = true;

            // // Handle PC changes
            // if (regNumber == 0)
            // {
            //     // Start programming process
            //     McManager::targetProgrammingStart(true);

            //     // Step to initiate programming and reset
            //     TargetTracker::completeTargetProgram();
            // }

            // Response
            _respBuffer[5] = 0;
            _respBufferPos += 1;
            break;
        }
        case CMD_WRITE_BANK:
        {
            if (_rxCmdBufferPos < 8199)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd writeBankTooShort");
                return PROC_CMD_PARAM_ERROR;
            }
            uint32_t bankIdx = _rxCmdBuffer[6];
            uint32_t linearBaseAddr = getAddrFrom8KBank_InitZXSpectrumNext(bankIdx);
            LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd writeBank bankIdx %d bankLen %d baseAddr %04x", 
                            bankIdx, _rxCmdBufferPos - 7, linearBaseAddr);
            
            // Write the block
            // HwManager::blockWrite(linearBaseAddr, _rxCmdBuffer+7, 8192, false, false, false);
            _mcManager.getTargetProgrammer().addMemoryBlockStatic(linearBaseAddr, _rxCmdBuffer+7, 8192,
                        &_mcManager.getTargetProgrammer());
            break;
        }
        // TODO
        // case CMD_CONTINUE:
        // {
        //     // Check valid
        //     if (_rxCmdBufferPos < 16)
        //     {
        //         LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd cmdContinueTooShort");
        //         return PROC_CMD_PARAM_ERROR;
        //     }

        //     // Get params
        //     const uint8_t* pBuf = _rxCmdBuffer + 6;
        //     bool enableBreakpoint1 = Utils::getUint8AndInc(pBuf, NULL) != 0;
        //     uint32_t breakpointAddr1 = Utils::getLEUint16AndInc(pBuf, NULL);
        //     bool enableBreakpoint2 = Utils::getUint8AndInc(pBuf, NULL) != 0;
        //     uint32_t breakpointAddr2 = Utils::getLEUint16AndInc(pBuf, NULL);
        //     uint32_t altCommand = Utils::getUint8AndInc(pBuf, NULL) != 0;
        //     uint32_t stepOverRangeStart = Utils::getLEUint16AndInc(pBuf, NULL);
        //     uint32_t stepOverRangeEnd = Utils::getLEUint16AndInc(pBuf, NULL);

        //     // Step over
        //     if (enableBreakpoint1)
        //     {
        //         // TargetTracker::stepTo(breakpointAddr1);
        //         TargetTracker::stepInto();
        //     }
        //     else if (altCommand == 2)
        //     {
        //         // Step out
        //         TargetTracker::stepOver();
        //     }
        //     _stepCompletionPending = true;
        //     // Debug
        //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "procCmd continue");
        //     break;
        // }
        case CMD_READ_MEM:
        {
            if (_rxCmdBufferPos < 10)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd readMemoryTooShort");
                return PROC_CMD_PARAM_ERROR;
            }

            // Skip reserved
            pCmdBuf++;

            // Get params
            uint32_t memAddr = Utils::getLEUint16AndInc(pCmdBuf, pCmdBufEnd);
            uint32_t memSize = Utils::getLEUint16AndInc(pCmdBuf, pCmdBufEnd);
            LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd readMem memAddr %04x memSize %04x", 
                            memAddr, memSize);

            static const uint32_t MAX_BYTES_TO_RETURN = 1024;
            uint32_t bytesRemaining = memSize;
            for (uint32_t rdAddr = memAddr; rdAddr < memAddr + memSize; rdAddr += MAX_BYTES_TO_RETURN)
            {
                // Read in chunks to avoid overrunning buffers
                uint8_t dataChunk[MAX_BYTES_TO_RETURN];
                uint32_t bytesToRead = bytesRemaining > MAX_BYTES_TO_RETURN ? MAX_BYTES_TO_RETURN : bytesRemaining;
                _mcManager.getHwManager().blockRead(rdAddr, dataChunk, bytesToRead, false, false, false);
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "procCmd readMem start 0x%04x len %d", rdAddr, bytesToRead);

                // Check first chunk
                if (rdAddr == memAddr)
                {
                    respPayloadLen = memSize + 1;
                    uint32_t bufPos = 0;
                    Utils::setLEUint16AndInc(_respBuffer, bufPos, respPayloadLen);
                    memcpy(_respBuffer+5, dataChunk, bytesToRead);
                    // TODO
                    _commandHandler.sendWithJSON("dezog", "", msgIdx, _respBuffer, respPayloadLen + 4);
                }
                else
                {
                    // TODO
                    _commandHandler.sendWithJSON("dezog", "", msgIdx, dataChunk, bytesToRead);
                }
            }
            return PROC_CMD_NO_RESPONSE;
        }
        // TODO
        // case CMD_GET_SLOTS:
        // {
        //     // Get slots
        //     // Response
        //     for (uint32_t slotIdx = 0; slotIdx < DEZOG_NUM_MEMBANK_SLOTS; slotIdx++)
        //     {
        //         *(_rxCmdBuffer + slotIdx + _respBufferPos) = 0;
        //     }
        //     _respBufferPos += DEZOG_NUM_MEMBANK_SLOTS;
        //     LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd getSlots");
        //     break;
        // }
        case CMD_SET_SLOT:
        {
            // Check valid
            if (_rxCmdBufferPos < 5)
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd cmdSetSlotTooShort");
                return PROC_CMD_PARAM_ERROR;
            }
            // Set slot
            uint32_t slotIdx = _rxCmdBuffer[6];
            uint32_t bankIdx = _rxCmdBuffer[7];
            LogWrite(MODULE_PREFIX, LOG_WARNING, "procCmd setSlot slotIdx %d bankIdx %d", 
                            slotIdx, bankIdx);
            // Response
            _respBuffer[5] = 0;
            _respBufferPos += 1;
            break;
        }
        case CMD_SET_BORDER:
        {
            // Set border colour - assume ZX Spectrum specific?
            // Just send ok response
            break;
        }
        default:
        {
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Unknown DeZog command 0x%02x", _rxCmdOrResp);
            return PROC_CMD_PARAM_ERROR;
        }
    }

    // Payload len
    respPayloadLen = _respBufferPos - 4;
    _respBuffer[0] = respPayloadLen & 0xff;
    _respBuffer[1] = (respPayloadLen >> 8) & 0xff;

    return PROC_CMD_SEND_RESPONSE;

    // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "DeZog handle line %s", pCmd);

    // // Split
    // const char* cmdStr = strtok(pCmd, " ");
    // char* argStr = strtok(NULL, " ");
    // char* argStr2 = strtok(NULL, " ");
    // char* argRest = strtok(NULL, "");
    // if (cmdStr == NULL)
    //     cmdStr = "\n";

    // // Handle messages
    // if (commandMatch(cmdStr, "about"))
    // {
    //     strlcat(pResponse, "BusRaider RCP", maxResponseLen);
    // }
    // else if (commandMatch(cmdStr, "get-version"))
    // {
    //     strlcat(pResponse, "8.0", maxResponseLen);
    // }
    // else if (commandMatch(cmdStr, "check-extensions"))
    // {
    //     strlcat(pResponse, "Extensions available.", maxResponseLen);
    // }
    // else if (commandMatch(cmdStr, "get-current-machine"))
    // {
    //     		// 		if(machine == 'zx-80')
	// 			// 	this.machineType = MachineType.ZX80;
	// 			// else if(machine == 'zx-81')
	// 			// 	this.machineType = MachineType.ZX81;
	// 			// else if(machine == 'spectrum 16k')
	// 			// 	this.machineType = MachineType.SPECTRUM16K;
	// 			// else if(machine == 'spectrum 48k')
	// 			// 	this.machineType = MachineType.SPECTRUM48K;
	// 			// else if(machine == 'spectrum 128k')
	// 			// 	this.machineType = MachineType.SPECTRUM128K;
	// 			// else if(machine == 'tbblue')
	// 			// 	this.machineType = MachineType.TBBLUE;
    //     static const int MAX_MACHINE_NAME_LEN = 200;
    //     char machineNameMunged[MAX_MACHINE_NAME_LEN];
    //     strlcpy(machineNameMunged, McManager::getDescriptorTable()->machineName, MAX_MACHINE_NAME_LEN);
    //     if (strcasecmp(machineNameMunged, "ZX Spectrum") == 0)
    //         strlcpy(machineNameMunged, "Spectrum 48k", MAX_MACHINE_NAME_LEN);
    //     strlcat(pResponse, machineNameMunged, maxResponseLen);
    // }
    // else if (commandMatch(cmdStr, "set-debug-settings"))
    // {
    // }
    // else if (commandMatch(cmdStr, "smartload"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "Smartload");

    //     // Recombine args
    //     char argsCombined[DEZOG_CMD_MAX_LEN];
    //     strlcpy(argsCombined, argStr, DEZOG_CMD_MAX_LEN);
    //     if (argStr2)
    //         strlcat(argsCombined, argStr2, DEZOG_CMD_MAX_LEN);
    //     if (argRest)
    //         strlcat(argsCombined, argRest, DEZOG_CMD_MAX_LEN);

    //     // Extract the file name only
    //     char* fileNamePtr = argsCombined+strlen(argsCombined);
    //     const char* fileTypePtr = "";
    //     for (uint32_t i = 0; i < strlen(argsCombined); i++)
    //     {
    //         if (*fileNamePtr == '.')
    //             fileTypePtr = fileNamePtr+1;
    //         if ((*fileNamePtr == '\\') || (*fileNamePtr == '/'))
    //         {
    //             fileNamePtr++;
    //             break;
    //         }
    //         fileNamePtr--;
    //     }
    //     // Check valid
    //     if (strlen(fileNamePtr) > 0)
    //     {
    //         // Check for machine to process file
    //         const char* pMcName = McManager::getMachineForFileType(fileTypePtr);
    //         if (pMcName)
    //         {
    //             McManager::setMachineByName(pMcName);
    //             LogWrite(MODULE_PREFIX, LOG_DEBUG, "fileType %s setMachine %s", fileTypePtr, pMcName);
    //         }
    //         // Request the file from the ESP32
    //         static const int MAX_API_REQ_LEN = 200;
    //         char apiReqStr[MAX_API_REQ_LEN];
    //         strlcpy(apiReqStr, "sendfiletotargetbuffer//", MAX_API_REQ_LEN);
    //         strlcat(apiReqStr, fileNamePtr, MAX_API_REQ_LEN);
    //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "sendFileReq %s", apiReqStr);
    //         // Send command to ESP32
    //         _commandHandler.sendAPIReq(apiReqStr);
    //         _smartloadStartUs = micros();
    //         _smartloadInProgress = true;
    //         _smartloadMsgIdx = dezogMsgIndex;
    //         _smartloadStartDetected = false;
    //         // Don't reply now to give time for smartload to occur
    //         return true;
    //     }
    //     else
    //     {
    //         LogWrite(MODULE_PREFIX, LOG_NOTICE, "Failed to smartload file");
    //         strlcat(pResponse, "Failed to smartload file", maxResponseLen);
    //     }
    // }
    // else if (commandMatch(cmdStr, "get-registers"))
    // {
    //     // Format registers to return
    //     TargetTracker::getRegsFormatted(pResponse, maxResponseLen);
    // }
    // else if (commandMatch(cmdStr, "set-register"))
    // {
    // }
    // else if (commandMatch(cmdStr, "get-stack-backtrace"))
    // {
    // }
    // else if (commandMatch(cmdStr, "read-memory"))
    // {
    //     // Check for extended args
    //     int memAddn = 0;
    //     if (strlen(argStr) > 5)
    //     {
    //         char memAddnStr[10];
    //         memset(memAddnStr, 0, 10);
    //         for (int i = 0; i < 10; i++)
    //         {
    //             memAddnStr[i] = argStr[5+i];
    //             if (memAddnStr[i] == 0)
    //                 break;
    //         }
    //         memAddn = strtol(memAddnStr, NULL, 10);
    //         argStr[5] = 0;
    //     }
    //     uint32_t startAddr = strtol(argStr, NULL, 10) + memAddn;
    //     uint32_t blockLength = strtol(argStr2, NULL, 10);
    //     static const uint32_t MAX_BYTES_TO_RETURN = 1024;
    //     if ((startAddr <= HwManager::getMaxAddress()) && (blockLength <= MAX_BYTES_TO_RETURN))
    //     {
    //         uint8_t dataBlock[MAX_BYTES_TO_RETURN];
    //         HwManager::blockRead(startAddr, dataBlock, blockLength, false, false, false);
    //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "dataBlock %04x %02x %02x %02x %02x", 
    //                         startAddr, dataBlock[0], dataBlock[1], dataBlock[2], dataBlock[3]);
    //         for (uint32_t i = 0; i < blockLength; i++)
    //         {
    //             char chBuf[10];
    //             ee_sprintf(chBuf, "%02X", dataBlock[i]);
    //             strlcat(pResponse, chBuf, maxResponseLen);
    //         }
    //     }
    // }
    // else if (commandMatch(cmdStr, "write-memory"))
    // {
    // }
    // else if (commandMatch(cmdStr, "write-memory-raw"))
    // {
    // }
    // else if (commandMatch(cmdStr, "get-memory-pages"))
    // {
    // }
    // else if (commandMatch(cmdStr, "disassemble"))
    // {
    //     // Disassemble code at specified location
    //     uint8_t* pMirrorMemory = HwManager::getMirrorMemForAddr(0);
    //     if (pMirrorMemory)
    //     {
    //         uint32_t addr = strtol(argStr, NULL, 10);
    //         disasmZ80(pMirrorMemory, 0, addr, pResponse, INTEL, false, true);
    //         mungeDisassembly(pResponse);
    //     }
    //     // LogWrite(FromDebugger, LOG_VERBOSE, "disassemble %s %s %s %d %s", argStr, argStr2, argRest, addr, pResponse);
    // }
    // else if (commandMatch(cmdStr, "enable-breakpoints"))
    // {
    //     TargetTracker::enableBreakpoints(true);
    // }
    // else if (commandMatch(cmdStr, "disable-breakpoints"))
    // {
    //     TargetTracker::enableBreakpoints(false);
    // }
    // else if (commandMatch(cmdStr, "set-breakpoint"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "set breakpoint %s %s", argStr, argStr2);

    //     // Set breakpoint
    //     int breakpointIdx = strtol(argStr, NULL, 10) - 1;
    //     if ((argStr2[0] != 'P') || (argStr2[1] != 'C') || (argStr2[2] != '='))
    //     {
    //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "breakpoint format must be PC= argstr2 %02x %02x %02x", 
    //                     argStr2[0], argStr2[1], argStr2[2]);
    //     }
    //     else
    //     {
    //         int addr = strtol(argStr2+3, NULL, 16);
    //         TargetTracker::setBreakpointPCAddr(breakpointIdx, addr);
    //     }        
    // }
    // else if (commandMatch(cmdStr, "set-breakpointaction"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "set breakpoint action %s %s %s", argStr, argStr2, argRest);

    //     // Set action on breakpoint
    //     int breakpointIdx = strtol(argStr, NULL, 10) - 1;
    //     if (!commandMatch(argStr2, "prints"))
    //     {
    //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "breakpoint doesn't have message");
    //     }
    //     else
    //     {        
    //         TargetTracker::setBreakpointMessage(breakpointIdx, argRest);
    //     }
    // }
    // else if (commandMatch(cmdStr, "enable-breakpoint"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "enable breakpoint %s", argStr);

    //     // Get breakpoint to enable
    //     int breakpointIdx = strtol(argStr, NULL, 10) - 1;
    //     TargetTracker::enableBreakpoint(breakpointIdx, true);    
    // }
    // else if (commandMatch(cmdStr, "disable-breakpoint"))
    // {
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "disable breakpoint %s", argStr);

    //     // Breakpoint number
    //     int breakpointIdx = strtol(argStr, NULL, 10) - 1;
    //     TargetTracker::enableBreakpoint(breakpointIdx, false);    
    // }
    // else if (commandMatch(cmdStr, "clear-membreakpoints"))
    // {
    //     // TODO
    //     // TargetTracker::enableBreakpoints();
    // }
    // else if (commandMatch(cmdStr, "set-membreakpoint"))
    // {
    //     // TODO
    //     // TargetTracker::setMemBreakpoint();
    // }
    // else if (commandMatch(cmdStr, "clear-fast-breakpoint"))
    // {
    //     int breakpointAddr = strtol(argStr, NULL, 10);
    //     TargetTracker::setFastBreakpoint(breakpointAddr, false);
    // }
    // else if (commandMatch(cmdStr, "set-fast-breakpoint"))
    // {
    //     int breakpointAddr = strtol(argStr, NULL, 10);
    //     TargetTracker::setFastBreakpoint(breakpointAddr, true);
    // }
    // else if (commandMatch(cmdStr, "clear-all-fast-breakpoints"))
    // {
    //     TargetTracker::clearFastBreakpoints();
    // }
    // else if (commandMatch(cmdStr, "set-fast-watchpoint"))
    // {
    //     // TODO
    //     // TargetTracker::setWatchpoint();
    // }
    // else if (commandMatch(cmdStr, "hard-reset-cpu"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "Reset machine");
    //     BusAccess::busAccessReset();
    //     TargetTracker::targetReset();
    //     _resetPending = true;
    //     _resetPendingTimeUs = micros();
    // }
    // else if (commandMatch(cmdStr, "reset-tstates-partial"))
    // {
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Reset tstates partial");
    // }
    // else if (commandMatch(cmdStr, "get-tstates-partial"))
    // {
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Get tstates partial");
    // }
    // else if (commandMatch(cmdStr, "get-cpu-frequency"))
    // {
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Get CPU frequency");
    // }
    // else if (commandMatch(cmdStr, "quit"))
    // {
    //     // Target tracker off
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargetTracker OFF");
    //     TargetTracker::enable(false);
    // }
    // else if (commandMatch(cmdStr, "enter-cpu-step"))
    // {
    //     // Reset bus
    //     BusAccess::busAccessReset();
    //     // Target tracker on
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargetTracker ON");
    //     if (_resetPending)
    //     {
    //         if (!isTimeout(micros(), _resetPendingTimeUs, MAX_TIME_RESET_PENDING_US))
    //             McManager::targetReset();
    //         _resetPending = false;
    //     }
    //     TargetTracker::enable(true);
    // }
    // else if (commandMatch(cmdStr, "cpu-step-over"))
    // {
    //     // Step over
    //     TargetTracker::stepOver();
    //     _stepCompletionPending = true;
    //     // Debug
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepOver");
    //     // Return immediately (no prompt)
    //     return true;
    // }
    // else if (commandMatch(cmdStr, "cpu-step"))
    // {
    //     // Step into
    //     TargetTracker::stepInto();
    //     _stepCompletionPending = true;
    //     // Debug
    //     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "stepInto");
    //     // Return immediately (no prompt)
    //     return true;
    // }
    // else if (commandMatch(cmdStr, "run"))
    // {
    //     // Release target to run (still in debug mode as breakpoint maybe hit)
    //     TargetTracker::stepRun();
    //     _stepCompletionPending = true;

    //     // Reply without sending a prompt
    //     strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", 
    //                 maxResponseLen);
    //     return true;    
    // }
    // else if (commandMatch(cmdStr, "\n"))
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "blank (step)");
    //     TargetTracker::stepInto();
    // }
    // else
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "cmdStr %s %02x %02x %02x", cmdStr, cmdStr[0], cmdStr[1], cmdStr[2]);
    // }

    // // Add the prompt used by debugger to detect state
    // addPromptMsg(pResponse, maxResponseLen);

    // return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeZogInterface::mungeDisassembly(char* pText)
{
    // Make upper case
    int txtLen = strlen(pText);
    for (int i = 0; i < txtLen; i++)
        pText[i] = rdtoupper(pText[i]);

    // Remove comment and whitespace at end of line
    for (int i = txtLen-1; i > 0; i--)
    {
        if (rdisspace(pText[i]) || (pText[i] == ';'))
            pText[i] = 0;
        else
            break;
    }
}

bool DeZogInterface::commandMatch(const char* s1, const char* s2)
{
    const char* p1 = s1;
    const char* p2 = s2;
    // Skip blanks at start of received string
    while (*p1 == ' ')
        p1++;
    // Check match from start of received string
    while(*p1)
    {
        if (*p2 == 0)
        {
            while (rdisspace(*p1))
                p1++;
            return *p1 == 0;
        }
        if (*p1 == 0)
        {
            // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> FALSE s1 shorter", s1, s2);
            return false;
        }
        if (rdtolower(*p1++) != rdtolower(*p2++))
        {
            // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> FALSE no match at %d", s1, s2, p1 - s1);
            return false;
        }
    }
    // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> %s ", s1, s2, *p2 == 0 ? "TRUE" : "FALSE");
    return (*p2 == 0);
}

// // Bus Raider
// // Rob Dobson 2019

// #include <string.h>
// #include <stdlib.h>
// #include "Debugger.h"
// #include "../System/logging.h"
// #include "../System/piwiring.h"
// #include "../System/lowlib.h"
// #include "../System/rdutils.h"
// #include "../TargetBus/BusAccess.h"
// #include "../TargetBus/TargetCPUZ80.h"
// #include "../Disassembler/src/mdZ80.h"

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Variables
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Module name
// static const char FromDebugger[] = "Debugger";

// // Sockets
// int Debugger::_busSocketId = -1;
// int Debugger::_commsSocketId = -1;

// // Main comms socket - to wire up command handler
// CommsSocketInfo Debugger::_commsSocketInfo =
// {
//     true,
//     Debugger::handleRxMsg,
//     NULL,
//     NULL
// };

// // Step Tracer
// BusSocketInfo Debugger::_busSocketInfo = 
// {
//     false,
//     Debugger::handleWaitInterruptStatic,
//     Debugger::busActionCompleteStatic,
//     true,
//     true,
//     BR_BUS_ACTION_NONE,
//     1,
//     false,
//     BR_BUS_ACTION_DISPLAY,
//     false
// };

// // This instance
// Debugger* Debugger::_pThisInstance = NULL;

// // Debug status
// DebuggerStatus Debugger::_debugStatus;

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Breakpoint and step-over
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // bool Debugger::_breakpointsEnabled = true;
// // int Debugger::_breakpointNumEnabled = 0;
// // Breakpoint Debugger::_breakpoints[MAX_BREAKPOINTS];
// // int Debugger::_breakpointIdxsToCheck[MAX_BREAKPOINTS];
// // bool Debugger::_breakpointHitFlag = false;
// // int Debugger::_breakpointHitIndex = 0;
// // bool Debugger::_stepOverEnabled = false;
// // uint32_t Debugger::_stepOverPCValue = 0;
// // bool Debugger::_stepOverHit = false;

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Construction
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Debugger::Debugger()
// {
//     _pThisInstance = this;
//     _debugMode = DEBUG_MODE_NONE;
//     // _instrWaitRestoreNeeded = false;
//     // _instrWaitCurMode = false;
//     // _instrClockCurFreqHz = BR_TARGET_DEBUG_CLOCK_HZ;
//     // _registerMode = REGISTER_MODE_NONE;
//     // _registerPrevInstrComplete = false;
//     // _registerQueryWriteIndex = 0;
//     // _registerModeStep = 0;
//     // for (int i = 0; i < MAX_BREAKPOINTS; i++)
//     //     _breakpoints[i].enabled = false;
//     // _breakpointNumEnabled = 0;
//     // _breakpointsEnabled = true;
//     // _breakpointHitFlag = false;
//     // _stepOverEnabled = false;
//     // _stepOverHit = false;
//     // _breakpointHitIndex = 0;
//     // _registerSetCodeLen = 0;
//     // _lastInstructionWasPrefixed = false;
//     // _thisInstructionIsPrefixed = false;
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Setup
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void Debugger::init()
// {
//     // Connect to the bus socket
//     if (_busSocketId < 0)
//         _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);

//     // Connect to the comms socket
//     if (_commsSocketId < 0)
//         _commsSocketId = _commandHandler.commsSocketAdd(_commsSocketInfo);
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Execution control
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void Debugger::pause()
// {
//     // targetStateAcqStart(true);
// }

// void Debugger::step([[maybe_unused]]bool stepOver)
// {

// }

// void Debugger::release()
// {

// }

// bool Debugger::isPaused()
// {
//     // return _debugMode == DEBUG_MODE_STEP_INTO;
//     return false;
// }

// // // Inform target debug that target programming is started
// // void Debugger::targetProgrammingStarted()
// // {

// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Received message handler
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool Debugger::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
//                 [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
// {
//         //TODO - from McManager - need to strip off rdp,etc
//     // else if (strcasecmp(cmdName, "rdp") == 0)
//     // {
//     //     // Get message index value
//     //     static const int MAX_RDP_MSG_ID_LEN = 20;
//     //     char rdpMessageIdStr[MAX_RDP_MSG_ID_LEN+1];
//     //     strcpy(rdpMessageIdStr, "0");
//     //     if (!jsonGetValueForKey("index", pCmdJson, rdpMessageIdStr, MAX_RDP_MSG_ID_LEN))
//     //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "RDP message no index value found");
//     //     // Send to remote debug handler
//     //     static const int MAX_CMD_STR_LEN = 200;
//     //     static char commandStr[MAX_CMD_STR_LEN+1];
//     //     if (paramsLen > MAX_CMD_STR_LEN)
//     //         paramsLen = MAX_CMD_STR_LEN;
//     //     memcpy(commandStr, pParams, paramsLen);
//     //     commandStr[paramsLen] = 0;
//     //     static const int MAX_RESPONSE_MSG_LEN = 2000;
//     //     static char responseMessage[MAX_RESPONSE_MSG_LEN+1];
//     //     responseMessage[0] = 0;
//     //     McManager::debuggerCommand(commandStr, responseMessage, MAX_RESPONSE_MSG_LEN);
//     //     // Send response back
//     //     sendRemoteDebugProtocolMsg(responseMessage, rdpMessageIdStr);
//     // }

//     // Get the command string from JSON
//     static const int MAX_CMD_NAME_STR = 50;
//     char cmdName[MAX_CMD_NAME_STR+1];
//     if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
//         return false;

//     // Check for debug commands
//     if (strcasecmp(cmdName, "stepStart") == 0)
//     {
//         // Stop step pause
//         _debugStatus._stepPause = false;
//         BusAccess::busSocketEnable(_busSocketId, true);
//         strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
//         return true;
//     }
//     else if (strcasecmp(cmdName, "stepStop") == 0)
//     {
//         // step pause
//         _debugStatus._stepPause = false;
//         BusAccess::busSocketEnable(_busSocketId, false);
//         strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
//         return true;
//     }
//     else if (strcasecmp(cmdName, "stepPause") == 0)
//     {
//         // Step pause
//         _debugStatus._stepPause = true;
//         strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
//         return true;
//     }
//     else if (strcasecmp(cmdName, "stepRelease") == 0)
//     {
//         // Step pause
//         _debugStatus._stepPause = false;
//         BusAccess::waitRelease();
//         strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
//         return true;
//     }
//     else if (strcasecmp(cmdName, "stepSingle") == 0)
//     {
//         // Single step
//         BusAccess::waitRelease();
//         strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
//         return true;
//     }    
//     return false;
// }

// // /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // // Target state acquisition - state machine
// // /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // void Debugger::targetStateAcqClear()
// // {
// //     _targetStateAcqMode = TARGET_STATE_ACQ_NONE;
// //     // _debugInCPUStep = false;
// //     _debugMode = DEBUG_MODE_NONE;
// // }

// // void Debugger::targetStateAcqStart([[maybe_unused]]bool leaveInPause)
// // {
// //     // Start the state machine to machine state

// //     // Check if bus access required at start
// //     // if (HwManager::pagingRequiresBusAccess() && !RAMEmulator::isActive())
// //     //     _targetStateAcqMode = TARGET_STATE_ACQ_PRE_INJECT;
// //     // else
// //     //     _targetStateAcqMode = TARGET_STATE_ACQ_INJECT;

// //     //             // Pause execution
// //     //     pause();
// //     //     BusAccess::waitOnInstruction(true);
// //     //     BusAccess::clockSetFreqHz(BR_TARGET_DEBUG_CLOCK_HZ);
// //     //     _lastInstructionWasPrefixed = false;


// //     //     _debugInCPUStep = true;
// //     //     _debugMode = ????

// //     // // BLANK
// //     //             pause();

// //     //         // Start sequence of getting registers
// //     //         startGetRegisterSequence();

// //     //         // LogWrite(FromDebugger, LOG_VERBOSE, "now paused SEND-BLANK");
// //     //         _debugInCPUStep = true;



// // }

// // void Debugger::targetStateAcqRelease(DEBUG_MODE debugMode)
// // {

// //     // BusAccess::waitRestoreDefaults();
// //     // BR_RETURN_TYPE retc = McManager::targetRelease();
// //     // _debugInCPUStep = false;
// //     // if (retc != BR_OK)
// //     //     LogWrite(FromDebugger, LOG_DEBUG, "pauseRelease failed in exit-cpu-step (%s)", BusAccess::retcString(retc));


// //     //     // STEP
// //     //     BR_RETURN_TYPE retc = McManager::targetStep();
// //     //     if (retc == BR_OK)
// //     //     {
// //     //         // Start sequence of getting registers
// //     //         startGetRegisterSequence();
// //     //         // LogWrite(FromDebugger, LOG_VERBOSE, "cpu-step done");
// //     //         _debugInCPUStep = true;

// //     //     }
// //     //     else
// //     //     {
// //     //         LogWrite(FromDebugger, LOG_DEBUG, "cpu-step failed (%s)", BusAccess::retcString(retc));
// //     //     }


// //     //     // STEPOVER
// //     //             _stepOverEnabled = true;
// //     //     _stepOverHit = false;
// //     //     // Release execution
// //     //     BR_RETURN_TYPE retc = McManager::targetRelease();
// //     //     _debugInCPUStep = false;
// //     //     if (retc != BR_OK)
// //     //         LogWrite(FromDebugger, LOG_DEBUG, "cpu-step-over pauseRelease failed (%s)", BusAccess::retcString(retc));


// //     _debugMode = debugMode;
// // }

// // void Debugger::targetStateAcqService()
// // {
// //     // Check for memory grab required
// //     // if (_busControlRequestedForMemGrab)
// //     // {
// //     //     // No longer request
// //     //     _busControlRequestedForMemGrab = false;

// //     //     // Release paging
// //     //     HwManager::pageOutForInjection(false);

// //     //     // Request bus so we can grab memory data before entering a wait state
// //     //     if (BusAccess::controlRequestAndTake() == BR_OK)
// //     //         BusAccess::grabMemoryAndReleaseBusRq(RAMEmulator::getMemBuffer(), RAMEmulator::getMemBufferLen());
// //     // }
// // }

// // uint32_t Debugger::targetStateAcqWaitISR([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
// //             [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
// // {
// //     // switch(_targetStateAcqMode)
// //     // {
// //     //     case TARGET_STATE_ACQ_NONE:
// //     //     {
// //     //         break;
// //     //     }
// //     //     case TARGET_STATE_ACQ_PRE_INJECT:
// //     //     {
// //     //         break;
// //     //     }
// //     // }


// // //     if (mode == START GETTING REGS)
// // //     {
// // //             // Start sequence of getting registers
// // //         _registerMode = REGISTER_MODE_GET;
// // //         _registerPrevInstrComplete = false;
// // //         _registerQueryWriteIndex = 0;
// // //         _registerModeStep = 0;
// // //     }

// // // #else
// // //     // Check for the end of paging mode - un-page RAM
// // //     if (_registerMode == REGISTER_MODE_UNPAGE)
// // //     {
// // //         // Clear the paging of RAM
// // //         HwManager::pageOutForInjection(false);
// // //         _registerMode = REGISTER_MODE_NONE;

// // //         // Restore wait-state generation if required
// // //         if (_instrWaitRestoreNeeded)
// // //         {
// // //             BusAccess::waitOnInstruction(_instrWaitCurMode);
// // //             BusAccess::clockSetFreqHz(_instrClockCurFreqHz);            
// // //             _instrWaitRestoreNeeded = false;
// // //         }

// // //         // Handle M1 cycle
// // //         _registerPrevInstrComplete = false;
// // //         if (flags & BR_CTRL_BUS_M1_MASK)
// // //         {
// // //             // Always need to hold at this point
// // //             retVal |= BR_MEM_ACCESS_HOLD;
// // //         }
// // //         else
// // //         {
// // //             // After regsiter set/get there should always be an M1 cycle
// // //TODO
// // //             ISR_ASSERT(ISR_ASSERT_CODE_UNPAGE_NOT_M1);
// // //         }
// // //     }

// // //     // Handle register query and set modes
// // //     if ((_registerMode == REGISTER_MODE_GET) || (_registerMode == REGISTER_MODE_SET))
// // //     {
// // //         // See if we are waiting for the end of the previous instruction
// // //         if (_registerPrevInstrComplete || ((!_lastInstructionWasPrefixed) && (flags & BR_CTRL_BUS_M1_MASK)))
// // //         {
// // //             //TODO
// // //                 // for (int i = 0; i < 5; i++)
// // //                 // {
// // //                 //     digitalWrite(8,1);
// // //                 //     microsDelay(1);
// // //                 //     digitalWrite(8,0);
// // //                 //     microsDelay(1);
// // //                 // }
// // //             // If we've just completed the previous instruction then the current address is the PC
// // //             if ((!_registerPrevInstrComplete) && (_registerMode == REGISTER_MODE_GET))
// // //             {
// // //                 //     digitalWrite(BR_PUSH_ADDR_BAR, 1);
// // //                 // int addrMask = 0x8000;
// // //                 // digitalWrite(8,1);
// // //                 // microsDelay(1);
// // //                 // for (int i = 0; i < 16; i++)
// // //                 // {
// // //                 //     digitalWrite(8,addr & addrMask);
// // //                 //     microsDelay(1);
// // //                 //     addrMask = addrMask >> 1;
// // //                 // }
// // //                 // digitalWrite(8,0);
// // //                 // microsDelay(1);
// // //                 _z80Registers.PC = addr;
// // //             }

// // //             // Previous instruction now done
// // //             _registerPrevInstrComplete = true;

// // //             // Page-out RAM/ROM while injecting
// // //             HwManager::pageOutForInjection(true);

// // //             // Handle the injection mode
// // //             if (_registerMode == REGISTER_MODE_GET)
// // //             {
// // //                 // We're now inserting instructions or getting results back
// // //                 handleRegisterGet(addr, data, flags, retVal);
// // //             }
// // //             else
// // //             {
// // //                 // We're now inserting instructions to set registers
// // //                 handleRegisterSet(retVal);
// // //             }

// // //             // If register get/set is finished after this cycle then request the bus (if not fully emulating RAM)
// // //             if ((_registerMode == REGISTER_MODE_UNPAGE) && (!RAMEmulator::isActive()))
// // //             {
// // //                 BusAccess::controlRequest();
// // //                 _busControlRequestedForMemGrab = true;
// // //             }
// // //         }
// // //         else
// // //         {
// // //             // Check if this is a prefix instruction - if so we need to avoid breaking at the wrong time
// // //             _lastInstructionWasPrefixed &= isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
// // //         }
// // //     }

// //     return retVal;
// // }

// // /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // // Grab all physical memory and release bus
// // /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // void Debugger::grabMemoryAndReleaseBusRq(uint8_t* pBuf, uint32_t bufLen)
// // {
// //     // Check if the bus in under BusRaider control
// //     if (BusAccess::isUnderControl())
// //     {
// //         // Get the current wait enablement and disable MREQ waits
// //         bool curMonitorIORQ = false;
// //         bool curMonitorMREQ = false;
// //         BusAccess::waitGet(curMonitorIORQ, curMonitorMREQ);
// //         BusAccess::waitOnInstruction(false);

// //         // Read all of memory
// //         BusAccess::blockRead(0, pBuf, bufLen, false, false);

// //         // Restore wait enablement
// //         BusAccess::waitOnInstruction(curMonitorMREQ);
        
// //         // Release control of bus
// //         BusAccess::controlRelease(false, false);
// //     }
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Breakpoints
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // void Debugger::enableBreakpoint(int idx, bool enabled)
// // {
// //     if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
// //         return;
// //     _breakpoints[idx].enabled = enabled;
// //     _breakpointNumEnabled = 0;
// //     int numBreakpointsEnabled = 0;
// //     for (int i = 0; i < MAX_BREAKPOINTS; i++)
// //         if (_breakpoints[i].enabled)
// //             _breakpointIdxsToCheck[numBreakpointsEnabled++] = i;
// //     _breakpointNumEnabled = numBreakpointsEnabled;
// // }

// // void Debugger::setBreakpointMessage(int idx, const char* hitMessage)
// // {
// //     if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
// //         return;
// //     if (hitMessage != NULL)
// //         strlcpy(_breakpoints[idx].hitMessage, hitMessage, Breakpoint::MAX_HIT_MSG_LEN);
// // }

// // void Debugger::setBreakpointPCAddr(int idx, uint32_t pcVal)
// // {
// //     if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
// //         return;
// //     _breakpoints[idx].pcValue = pcVal;
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Injected instruction sequence handling for register set
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // void Debugger::startSetRegisterSequence(Z80Registers* pRegs)
// // {
// //     //TODO
// //     // // Set regs
// //     // if (pRegs)
// //     //     _z80Registers = *pRegs;

// //     // bool curMonitorIORQ = false;
// //     // BusAccess::waitGet(curMonitorIORQ, _instrWaitCurMode);
// //     // _instrClockCurFreqHz = BusAccess::clockCurFreqHz();
// //     // LogWrite(FromDebugger, LOG_VERBOSE, "startSetRegisterSequence curMonitorInstr %s",
// //     //             _instrWaitCurMode ? "Y" : "N");

// //     // // Put machine into wait-state mode for MREQ
// //     // if (!_instrWaitCurMode)
// //     //     _lastInstructionWasPrefixed = false;
// //     // _instrWaitRestoreNeeded = true;
// //     // BusAccess::waitOnInstruction(true);
// //     // BusAccess::clockSetFreqHz(BR_TARGET_DEBUG_CLOCK_HZ);

// //     // // Start sequence of setting registers
// //     // _registerMode = REGISTER_MODE_SET;
// //     // _registerPrevInstrComplete = false;
// //     // _registerModeStep = 0;
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Utility functions
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // bool Debugger::commandMatch(const char* s1, const char* s2)
// // {
// //     const char* p1 = s1;
// //     const char* p2 = s2;
// //     // Skip whitespace at start of received string
// //     while (*p1 == ' ')
// //         p1++;
// //     // Check match from start of received string
// //     while(*p1)
// //     {
// //         if (*p2 == 0)
// //         {
// //             while (rdisspace(*p1))
// //                 p1++;
// //             return *p1 == 0;
// //         }
// //         if (*p1 == 0)
// //         {
// //             // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> FALSE s1 shorter", s1, s2);
// //             return false;
// //         }
// //         if (rdtolower(*p1++) != rdtolower(*p2++))
// //         {
// //             // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> FALSE no match at %d", s1, s2, p1 - s1);
// //             return false;
// //         }
// //     }
// //     // LogWrite(FromDebugger, LOG_VERBOSE, "Compare <%s> <%s> %s ", s1, s2, *p2 == 0 ? "TRUE" : "FALSE");
// //     return (*p2 == 0);
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Service - to handle breakpoint hits
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void Debugger::service()
// {
//     // // Service the target state acquisition state-machine
//     // targetStateAcqService();

//     // // Check for breakpoint hit
//     // if (_breakpointHitFlag)
//     // {
//     //     LogWrite(FromDebugger, LOG_VERBOSE, "breakpoint hit %d debug", _breakpointHitIndex);
//     //     if (_pSendRemoteDebugProtocolMsgCallback)
//     //         (*_pSendRemoteDebugProtocolMsgCallback)("\ncommand@cpu-step> ", "0");
//     //     _breakpointHitFlag = false;
//     // }

//     // // Check for step-over hit
//     // if (_stepOverHit)
//     // {
//     //     static const int MAX_DISASSEMBLY_LINE_LEN = 200;
//     //     char respBuf[MAX_DISASSEMBLY_LINE_LEN];
//     //     LogWrite(FromDebugger, LOG_VERBOSE, "step-over hit");
//     //     disasmZ80(RAMEmulator::getMemBuffer(), 0, _stepOverPCValue, respBuf, INTEL, false, true);
//     //     strlcat(respBuf, "\ncommand@cpu-step> ", MAX_DISASSEMBLY_LINE_LEN);
//     //     if (_pSendRemoteDebugProtocolMsgCallback)
//     //         (*_pSendRemoteDebugProtocolMsgCallback)(respBuf, "0");
//     //     _stepOverHit = false;
//     // }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Debugger command handler - based on DeZog / Z80 Debugger for VS Code
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // bool Debugger::handleDebuggerCommand(char* pCommand, char* pResponse, int maxResponseLen)
// // {
// //     // Empty response initially
// //     pResponse[0] = 0;

// //     // Break into lines
// //     char* pCmdCur = pCommand;
// //     char* pCmdNext = pCmdCur;
// //     while (*pCmdCur)
// //     {
// //         // Find end of command string
// //         while(*pCmdNext)
// //         {
// //             if (*pCmdNext == '\n')
// //             {
// //                 // Replace newline with terminator
// //                 *pCmdNext = 0;
// //                 // Move to next command (or null)
// //                 pCmdNext++;
// //                 break;
// //             }
// //             pCmdNext++;
// //         }

// //         // Process the command
// //         procDebuggerLine(pCmdCur, pResponse, maxResponseLen);

// //         // Next command
// //         pCmdCur = pCmdNext;
// //     }
// //     return true;
// // }

// // bool Debugger::procDebuggerLine(char* pCmd, char* pResponse, int maxResponseLen)
// // {
// //     // Split
// //     char* cmdStr = strtok(pCmd, " ");
// //     // // Trim command string
// //     // int j = 0;
// //     // for (size_t i = 0; i < strlen(cmdStr); i++)
// //     // {
// //     //     if (!rdisspace(cmdStr[i]))
// //     //     {
// //     //         cmdStr[j++] = cmdStr[i];
// //     //     }
// //     // }
// //     // cmdStr[j] = 0;
// //     // McManager::logDebugMessage(pCmd);

// //     char* argStr = strtok(NULL, " ");
// //     char* argStr2 = strtok(NULL, " ");
// //     char* argRest = strtok(NULL, "");

// //     if (commandMatch(cmdStr, "about"))
// //     {
// //         strlcat(pResponse, "BusRaider RCP", maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "get-version"))
// //     {
// //         strlcat(pResponse, "7.2-SN", maxResponseLen);
// //     }
// //     // TODO
// //     // else if (commandMatch(cmdStr, "get-current-machine"))
// //     // {
// //     //     strlcat(pResponse, McManager::getDescriptorTable()->machineName, maxResponseLen);
// //     // }
// //     else if (commandMatch(cmdStr, "set-debug-settings"))
// //     {
// //     }
// //     else if (commandMatch(cmdStr, "hard-reset-cpu"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "Reset machine");
// //         // TODO
// //         // McManager::targetReset(false, false);
// //         targetStateAcqClear();
// //     }
// //     else if (commandMatch(cmdStr, "enter-cpu-step"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "enter-cpu-step");

// //         // Acquire target state and hold
// //         targetStateAcqStart(true);
// //     }
// //     else if (commandMatch(cmdStr, "exit-cpu-step"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "exit-cpu-step");

// //         // Release target
// //         targetStateAcqRelease(DEBUG_MODE_NONE);
// //     }
// //     else if (commandMatch(cmdStr, "quit"))
// //     {
// //         // TODO
// //         // // LogWrite(FromDebugger, LOG_VERBOSE, "quit");
// //         // BusAccess::waitRestoreDefaults();

// //         // Release target
// //         targetStateAcqRelease(DEBUG_MODE_NONE);
// //     }
// //     else if (commandMatch(cmdStr, "smartload"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "smartload %s", argStr);

// //         // TODO
// //         // // Program the target
// //         // McManager::targetProgrammingStart(true, false, true, true);
// //     }
// //     else if (commandMatch(cmdStr, "clear-membreakpoints"))
// //     {
// //     }
// //     else if (commandMatch(cmdStr, "enable-breakpoint"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "enable breakpoint %s", argStr);

// //         // Get breakpoint to enable
// //         int breakpointIdx = strtol(argStr, NULL, 10) - 1;
// //         enableBreakpoint(breakpointIdx, true);
// //     }
// //     else if (commandMatch(cmdStr, "enable-breakpoints"))
// //     {
// //         // Enable
// //         _breakpointsEnabled = true;
// //     }
// //     else if (commandMatch(cmdStr, "disable-breakpoint"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "disable breakpoint %s", argStr);

// //         // Breakpoint number
// //         int breakpointIdx = strtol(argStr, NULL, 10) - 1;
// //         enableBreakpoint(breakpointIdx, false);
// //     }
// //     else if (commandMatch(cmdStr, "disable-breakpoints"))
// //     {
// //         _breakpointsEnabled = false;
// //         strlcat(pResponse, "", maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "set-breakpoint"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "set breakpoint %s %s", argStr, argStr2);

// //         // Set breakpoint
// //         int breakpointIdx = strtol(argStr, NULL, 10) - 1;
// //         if ((argStr2[0] != 'P') || (argStr2[1] != 'C') || (argStr2[2] != '='))
// //         {
// //             LogWrite(FromDebugger, LOG_DEBUG, "breakpoint format must be PC= argstr2 %02x %02x %02x", 
// //                         argStr2[0], argStr2[1], argStr2[2]);
// //         }
// //         else
// //         {
// //             int addr = strtol(argStr2+3, NULL, 16);
// //             setBreakpointPCAddr(breakpointIdx, addr);
// //         }        
// //     }
// //     else if (commandMatch(cmdStr, "set-breakpointaction"))
// //     {
// //         LogWrite(FromDebugger, LOG_VERBOSE, "set breakpoint action %s %s %s", argStr, argStr2, argRest);

// //         // Set action on breakpoint
// //         int breakpointIdx = strtol(argStr, NULL, 10) - 1;
// //         if (!commandMatch(argStr2, "prints"))
// //         {
// //             LogWrite(FromDebugger, LOG_DEBUG, "breakpoint doesn't have message");
// //         }
// //         else
// //         {        
// //             setBreakpointMessage(breakpointIdx, argRest);
// //         }
// //     }
// //     else if (commandMatch(cmdStr, "disassemble"))
// //     {
// //         // Disassemble at current location
// //         int addr = strtol(argStr, NULL, 10);
// //         disasmZ80(RAMEmulator::getMemBuffer(), 0, addr, pResponse, INTEL, false, true);
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "disassemble %s %s %s %d %s", argStr, argStr2, argRest, addr, pResponse);
// //     }
// //     else if (commandMatch(cmdStr, "get-registers"))
// //     {
// //         // Format registers to return
// //         _z80Registers.format(pResponse, maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "read-memory"))
// //     {
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "read mem %s %s", argStr, argStr2);
// //         int memAddn = 0;
// //         if (strlen(argStr) > 5)
// //         {
// //             char memAddnStr[10];
// //             memset(memAddnStr, 0, 10);
// //             for (int i = 0; i < 10; i++)
// //             {
// //                 memAddnStr[i] = argStr[5+i];
// //                 if (memAddnStr[i] == 0)
// //                     break;
// //             }
// //             memAddn = strtol(memAddnStr, NULL, 10);
// //             argStr[5] = 0;
// //         }
// //         int startAddr = strtol(argStr, NULL, 10) + memAddn;
// //         int leng = strtol(argStr2, NULL, 10);
// //         if (startAddr >= 0 && startAddr <= RAMEmulator::getMemBufferLen())
// //         {
// //             for (int i = 0; i < leng; i++)
// //             {
// //                 char chBuf[10];
// //                 ee_sprintf(chBuf, "%02x", RAMEmulator::getMemoryByte(startAddr+i));
// //                 strlcat(pResponse, chBuf, maxResponseLen);
// //             }
// //         }
// //     }
// //     else if (commandMatch(cmdStr, "get-tstates-partial"))
// //     {
// //         strlcat(pResponse, "Unknown command get-tstates-partial", maxResponseLen);
// //     }    
// //     else if (commandMatch(cmdStr, "check-extensions"))
// //     {
// //         strlcat(pResponse, "Unknown command check-extensions", maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "reset-tstates-partial"))
// //     {
// //         strlcat(pResponse, "Unknown command reset-tstates-partial", maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "get-cpu-frequency"))
// //     {
// //         strlcat(pResponse, "Unknown command get-cpu-frequency", maxResponseLen);
// //     }
// //     else if (commandMatch(cmdStr, "get-stack-backtrace"))
// //     {
// //         int startAddr = _z80Registers.SP;
// //         int numFrames = strtol(argStr, NULL, 10);
// //         if ((numFrames > 0) && (numFrames < MAX_MEM_DUMP_LEN / 2))
// //         {
// //             for (int i = 0; i < numFrames; i++)
// //             {
// //                 char chBuf[20];
// //                 ee_sprintf(chBuf, "%04xH ", RAMEmulator::getMemoryWord(startAddr + i*2));;
// //                 strlcat(pResponse, chBuf, maxResponseLen);
// //             }
// //         }
// //     }
// //     else if (commandMatch(cmdStr, "run"))
// //     {
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "run");

// //         // Release target to run (still in debug mode as breakpoint maybe hit)
// //         targetStateAcqRelease(DEBUG_MODE_RUN);

// //         // Reply without sending a prompt
// //         strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", 
// //                     maxResponseLen);
// //         return true;
// //     }
// //     else if (commandMatch(cmdStr, "cpu-step"))
// //     {
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "cpu-step");

// //         // Disassemble to generate response
// //         disasmZ80(RAMEmulator::getMemBuffer(), 0, _z80Registers.PC, pResponse, INTEL, false, true);

// //         // Release target for a single instruction
// //         targetStateAcqRelease(DEBUG_MODE_STEP_INTO);
// //     }
// //     else if (commandMatch(cmdStr, "cpu-step-over"))
// //     {
// //         // Get address to run to by disassembling code at current location
// //         int instrLen = disasmZ80(RAMEmulator::getMemBuffer(), 0, _z80Registers.PC, pResponse, INTEL, false, true);
// //         _stepOverPCValue = _z80Registers.PC + instrLen;
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "cpu-step-over PCnow %04x StepToPC %04x", _z80Registers.PC, _stepOverPCValue);

// //         // Release target for step over
// //         targetStateAcqRelease(DEBUG_MODE_STEP_OVER);

// //         // Return immediately (no prompt)
// //         return true;
// //     }
// //     else if (commandMatch(cmdStr, ""))
// //     {
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "blank (step) %s", _debugInitalized ? "" : "not in debug mode");
// //         if (_debugMode != DEBUG_MODE_NONE)
// //         {
// //             // Blank is used for pause
// //             targetStateAcqStart(true);
// //         }
// //     }
// //     else
// //     {
// //         LogWrite(FromDebugger, LOG_DEBUG, "unknown command %s", cmdStr);
// //         // for (unsigned int i = 0; i < strlen(pCommand); i++)
// //         // {
// //         //     char chBuf[10];
// //         //     ee_sprintf(chBuf, "%02x ", pCommand[i]);
// //         //     strlcat(pResponse, chBuf, maxResponseLen);
// //         // }
// //         strlcat(pResponse, "Unknown command", maxResponseLen);
// //     }
    
// //     // Complete and add command request
// //     // TODO
// //     // strlcat(pResponse, (McManager::targetIsPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);

// //     // LogWrite(FromDebugger, LOG_VERBOSE, "resp %s", pResponse);

// //     return true;
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Test is an instruction is a prefix
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // bool Debugger::isPrefixInstruction(uint32_t instr)
// // {
// //     switch(instr)
// //     {
// //         case 0xdd:
// //         case 0xed:
// //         case 0xfd:
// //         case 0xcb:
// //             return true;
// //     }
// //     return false;
// // }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Bus action callback
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void Debugger::busActionCompleteStatic([[maybe_unused]] BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
// {
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Interrupt extension for debugger
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void Debugger::handleWaitInterruptStatic([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
//         [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
// {
//     // Only handle MREQs
//     if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
//         return;

//     // if (_debugStatus._stepPause)
//     //     retVal = retVal | BR_MEM_ACCESS_HOLD;

//     // // Track M1 cycles to ensure we know when we're at the start of a new instruction
//     // // and not in the middle of a prefixed instruction sequence
//     // if (flags & BR_CTRL_BUS_M1_MASK)
//     // {
//     //     // Record state from previous instruction
//     //     _prefixTracker[0] = _prefixTracker[1];
//     //     // Check if this is a prefixed instruction
//     //     _prefixTracker[1] = isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
//     // }
//     // else
//     // {
//     //     // Can't be in the middle of a prefixed instruction if this isn't an M1 cycle
//     //     _thisInstructionIsPrefixed = false;
//     //     _lastInstructionWasPrefixed = false;
//     // }

//     // // // Breakpoint handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
//     // // if ((_breakpointsEnabled && (_breakpointNumEnabled > 0)) && 
//     // //         (_registerMode == REGISTER_MODE_NONE) &&
//     // //         (!_debugInCPUStep) &&
//     // //         (flags & BR_CTRL_BUS_M1_MASK) && 
//     // //         (!BusAccess::isUnderControl()))
//     // // {
//     // //     // Check breakpoints
//     // //     for (int i = 0; i < _breakpointNumEnabled; i++)
//     // //     {
//     // //         int bpIdx = _breakpointIdxsToCheck[i];
//     // //         if (_breakpoints[bpIdx].pcValue == addr)
//     // //         {
//     // //             BR_RETURN_TYPE retc = McManager::targetPause();
//     // //             if (retc == BR_OK)
//     // //                 startGetRegisterSequence();
//     // //             _z80Registers.PC = addr;
//     // //             _registerPrevInstrComplete = true;
//     // //             _debugInCPUStep = true;
//     // //             _breakpointHitFlag = true;
//     // //             _breakpointHitIndex = bpIdx;
//     // //             break;
//     // //         }
//     // //     }
//     // // }

//     // // // Step-over handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
//     // // if (_stepOverEnabled && 
//     // //         (_registerMode == REGISTER_MODE_NONE) &&
//     // //         (!_debugInCPUStep) &&
//     // //         (flags & BR_CTRL_BUS_M1_MASK) && 
//     // //         (!BusAccess::isUnderControl()))
//     // // {
//     // //     // Check stepover
//     // //     if (_stepOverPCValue == addr)
//     // //     {
//     // //         BR_RETURN_TYPE retc = McManager::targetPause();
//     // //         if (retc == BR_OK)
//     // //             startGetRegisterSequence();
//     // //         _z80Registers.PC = addr;
//     // //         _registerPrevInstrComplete = true;
//     // //         _debugInCPUStep = true;
//     // //         _stepOverHit = true;
//     // //         _stepOverEnabled = false;
//     // //     }
//     // // }

//     // // Handle targetStateAcquisition
//     // return targetStateAcqWaitISR(addr, data, flags, retVal);
// }

void DeZogInterface::setRegister(Z80Registers& regs, uint32_t regNumber, uint32_t val)
{
    switch(regNumber)
    {
        case 0: regs.PC = val; break;
        case 1: regs.SP = val; break;
        case 2: regs.AF = val; break;
        case 3: regs.BC = val; break;
        case 4: regs.DE = val; break;
        case 5: regs.HL = val; break;
        case 6: regs.IX = val; break;
        case 7: regs.IY = val; break;
        case 8: regs.AFDASH = val; break;
        case 9: regs.BCDASH = val; break;
        case 10: regs.DEDASH = val; break;
        case 11: regs.HLDASH = val; break;
        case 13: regs.INTMODE = val; break;
        case 14: regs.AF = (regs.AF & 0xf0) | val; break;
        case 15: regs.AF = (regs.AF & 0x0f) | (val << 8); break;
        case 16: regs.BC = (regs.BC & 0xf0) | val; break;
        case 17: regs.BC = (regs.BC & 0x0f) | (val << 8); break;
        case 18: regs.DE = (regs.DE & 0xf0) | val; break;
        case 19: regs.DE = (regs.DE & 0x0f) | (val << 8); break;
        case 20: regs.HL = (regs.HL & 0xf0) | val; break;
        case 21: regs.HL = (regs.HL & 0x0f) | (val << 8); break;
        case 22: regs.IX = (regs.IX & 0xf0) | val; break;
        case 23: regs.IX = (regs.IX & 0x0f) | (val << 8); break;
        case 24: regs.IY = (regs.IY & 0xf0) | val; break;
        case 25: regs.IY = (regs.IY & 0x0f) | (val << 8); break;
        case 26: regs.AFDASH = (regs.AFDASH & 0xf0) | val; break;
        case 27: regs.AFDASH = (regs.AFDASH & 0x0f) | (val << 8); break;
        case 28: regs.BCDASH = (regs.BCDASH & 0xf0) | val; break;
        case 29: regs.BCDASH = (regs.BCDASH & 0x0f) | (val << 8); break;
        case 30: regs.DEDASH = (regs.DEDASH & 0xf0) | val; break;
        case 31: regs.DEDASH = (regs.DEDASH & 0x0f) | (val << 8); break;
        case 32: regs.HLDASH = (regs.HLDASH & 0xf0) | val; break;
        case 33: regs.HLDASH = (regs.HLDASH & 0x0f) | (val << 8); break;
        case 34: regs.R = val; break;
        case 35: regs.I = val; break;
    }
}

uint32_t DeZogInterface::getRegisters(Z80Registers& regs, uint32_t bufPos)
{
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.PC);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.SP);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.AF);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.BC);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.DE);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.HL);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.IX);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.IY);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.AFDASH);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.BCDASH);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.DEDASH);
    Utils::setLEUint16AndInc(_respBuffer, bufPos, regs.HLDASH);
    Utils::setUint8AndInc(_respBuffer, bufPos, regs.R);
    Utils::setUint8AndInc(_respBuffer, bufPos, regs.I);
    Utils::setUint8AndInc(_respBuffer, bufPos, regs.INTMODE);
    Utils::setUint8AndInc(_respBuffer, bufPos, 0);
    return bufPos;
}

uint32_t DeZogInterface::getAddrFrom8KBank_InitZXSpectrumNext(uint32_t bankNumber8K)
{
    // Using information from https://wiki.specnext.dev/Memory_map
    static const uint32_t mapBankToAddr[] = {
        0x0c000, 0x0e000, 0x44000, 0x46000,
        0x08000, 0x0a000, 0x4c000, 0x4e000,
        0x50000, 0x52000, 0x04000, 0x06000,
        0x58000, 0x5a000, 0x5c000, 0x5e000
    };
    if (bankNumber8K < 16)
        return mapBankToAddr[bankNumber8K];
    return ((bankNumber8K - 16) * 0x2000) + 0x60000;
}
