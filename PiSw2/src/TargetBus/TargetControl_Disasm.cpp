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
