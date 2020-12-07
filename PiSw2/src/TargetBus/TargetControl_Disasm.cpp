// Bus Raider
// Rob Dobson 2019-2020

#include "TargetControl.h"
#include "mdZ80.h"
#include "BusControl.h"
#include "rdutils.h"

void TargetControl::disassemble(uint32_t numLines, char* pResp, uint32_t respMaxLen, 
            uint32_t& numBytesIn1StInstruction)
{
    // Disassembly
    uint32_t curAddr = _z80Registers.PC;
    uint8_t* pZ80Memory = _busControl.mem().getCacheMemPtr();
    strlcpy(pResp, "", respMaxLen);

    for (uint32_t i = 0; i < numLines; i++)
    {
        char tmpResp[1000];
        uint32_t bytesInInstr = disasmZ80(pZ80Memory, 0, curAddr, tmpResp, JUSTHEX, false, true, sizeof(tmpResp));
        // char* pTmpResp = mungeDisassembly(tmpResp);
        if (i == 0)
            numBytesIn1StInstruction = bytesInInstr;
        strlcat(pResp, tmpResp, respMaxLen);
        curAddr += bytesInInstr;
    }
}

    // mungeDisassembly(pRespMsg);
    // addPromptMsg(respMsg, DEZOG_RESP_MAX_LEN);
    // CommandHandler::sendWithJSON("dezog", "", _smartloadMsgIdx, 
    //                 (const uint8_t*)respMsg, strlen(respMsg));
    // _stepCompletionPending = false;



        // // Disassemble code at specified location
        // uint8_t* pMirrorMemory = HwManager::getMirrorMemForAddr(0);
        // if (pMirrorMemory)
        // {
        //     uint32_t addr = strtol(argStr, NULL, 10);
        //     disasmZ80(pMirrorMemory, 0, addr, pResponse, INTEL, false, true);
        //     mungeDisassembly(pResponse);            

// //         // Disassemble at current location
// //         int addr = strtol(argStr, NULL, 10);
// //         disasmZ80(RAMEmulator::getMemBuffer(), 0, addr, pResponse, INTEL, false, true);
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "disassemble %s %s %s %d %s", argStr, argStr2, argRest, addr, pResponse);


            //     //     static const int MAX_DISASSEMBLY_LINE_LEN = 200;
//     //     char respBuf[MAX_DISASSEMBLY_LINE_LEN];
//     //     LogWrite(FromDebugger, LOG_VERBOSE, "step-over hit");
//     //     disasmZ80(RAMEmulator::getMemBuffer(), 0, _stepOverPCValue, respBuf, INTEL, false, true);
//     //     strlcat(respBuf, "\ncommand@cpu-step> ", MAX_DISASSEMBLY_LINE_LEN);
//     //     if (_pSendRemoteDebugProtocolMsgCallback)
//     //         (*_pSendRemoteDebugProtocolMsgCallback)(respBuf, "0");
//     //     _stepOverHit = false;

// //         // Get address to run to by disassembling code at current location
// //         int instrLen = disasmZ80(RAMEmulator::getMemBuffer(), 0, _z80Registers.PC, pResponse, INTEL, false, true);
// //         _stepOverPCValue = _z80Registers.PC + instrLen;
// //         // LogWrite(FromDebugger, LOG_VERBOSE, "cpu-step-over PCnow %04x StepToPC %04x", _z80Registers.PC, _stepOverPCValue);


// char* TargetControl::mungeDisassembly(char* pText)
// {
//     // Make upper case
//     int txtLen = strlen(pText);
//     for (int i = 0; i < txtLen; i++)
//         pText[i] = rdtoupper(pText[i]);

//     // Remove whitespace and comment at start and end of line
//     char* pStart = pText;
//     for (int i = 0; i < txtLen-1; i++)
//     {
//         if (!rdisspace(*pStart))
//             break;
//         pStart++;
//     }
//     for (int i = txtLen-1; i > 0; i--)
//     {
//         if (rdisspace(pText[i]) || (pText[i] == ';'))
//         {
//             pText[i] = '/';
//             pText[i+1] = 0;
//         }
//         else
//         {
//             break;
//         }
//     }
//     return pStart;
// }
