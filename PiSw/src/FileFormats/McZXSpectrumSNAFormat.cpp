#include "../System/logging.h"
#include "McZXSpectrumSNAFormat.h"

const char* MODULE_PREFIX = "McSpecSNA";

// https://www.worldofspectrum.org/faq/reference/formats.htm
#pragma pack(push, 1)
typedef struct {
	uint8_t i;
	uint16_t hl2, de2, bc2, af2;
	uint16_t hl, de, bc;
	uint16_t iy, ix;
	uint8_t iff2;
	uint8_t r;
	uint16_t af, sp;
	uint8_t intMode;
	uint8_t borderColor;
	uint8_t ramDump[49152]; // RAM dump 16384 to 65535
} sna_t;
#pragma pack(pop)

McZXSpectrumSNAFormat::McZXSpectrumSNAFormat()
{
}

    // Info from https://www.worldofspectrum.org/faq/reference/formats.htm
bool McZXSpectrumSNAFormat::proc([[maybe_unused]]FileParserDataCallback* pDataCallback, 
            [[maybe_unused]] FileParserRegsCallback* pRegsCallback, 
            void* pCallbackParam,
            [[maybe_unused]] const uint8_t* pData, [[maybe_unused]] int dataLen)
{
    static const int SPECTRUM_SNA_FORMAT_BASE = 0x4000;

    if (dataLen < (int) sizeof(sna_t))
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Invalid format - not enough data\n");
        return false;
    }
    sna_t *sna = (sna_t *) pData;
    // Registers
    Z80Registers regs;
    regs.AF = sna->af;
    regs.AFDASH = sna->af2;
    regs.BC = sna->bc;
    regs.BCDASH = sna->bc2;
    regs.DE = sna->de;
    regs.DEDASH = sna->de2;
    regs.HL = sna->hl;
    regs.HLDASH = sna->hl2;
    regs.IX = sna->ix;
    regs.IY = sna->iy;
    regs.SP = sna->sp;
    regs.I = sna->i;
    regs.R = sna->r;
    regs.INTMODE = sna->intMode & 0x03;
    regs.INTENABLED = (sna->iff2 & 0x04) != 0;

    // PC is pushed onto the stack so get the PC value and restore stack
    uint32_t stackOffset = sna->sp - SPECTRUM_SNA_FORMAT_BASE;
    uint16_t pcVal = sna->ramDump[stackOffset] + sna->ramDump[stackOffset+1]*256;
    sna->ramDump[stackOffset] = 0;
    sna->ramDump[stackOffset+1] = 0;
    regs.SP++;
    regs.SP++;
    regs.PC = pcVal;
    pRegsCallback(regs, pCallbackParam);

    pDataCallback(SPECTRUM_SNA_FORMAT_BASE, sna->ramDump, 49152, pCallbackParam); 
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "format ok, PC %04x SP (after retn) %04x AF %04x HL %04x DE %04x BC %04x\n", 
                    regs.PC, regs.SP, regs.AF, regs.HL, regs.DE, regs.BC);
    return true;
}
