// Bus Raider Machine DebugZ80
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "ee_printf.h"

class McDebugZ80 : public McBase
{
private:
    static const char* _logPrefix;
    static constexpr uint32_t DEBUGZ80_RAM_ADDR = 0x0000;
    static constexpr uint32_t DEBUGZ80_RAM_SIZE = 0x10000;
    static uint8_t _systemRAM[DEBUGZ80_RAM_SIZE];
    static constexpr uint32_t DEBUGZ80_DISP_RAM_ADDR = 0xc000;
    static constexpr uint32_t DEBUGZ80_DISP_RAM_SIZE = 0x400;
    uint8_t _pScrnBufCopy[DEBUGZ80_DISP_RAM_SIZE];
    static volatile bool _scrnBufDirtyFlag;

    static McDescriptorTable _descriptorTable;

    static void handleExecAddr(uint32_t execAddr);

public:

    McDebugZ80();

    // Enable machine
    virtual void enable();

    // Disable machine
    virtual void disable();

    // Get descriptor table for the machine
    virtual McDescriptorTable* getDescriptorTable([[maybe_unused]] int subType)
    {
        return &_descriptorTable;
    }

    // Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
    virtual bool reset();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefresh();

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Handle a file
    virtual void fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

private:

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    static uint32_t memoryRequestCallback(uint32_t addr, uint32_t data, uint32_t flags);

};
