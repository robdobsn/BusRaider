// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McBase.h"

class McRobsZ80 : public McBase
{
private:
    static const char* _logPrefix;
    static constexpr uint32_t ROBSZ80_DISP_RAM_ADDR = 0x4000;
    static constexpr uint32_t ROBSZ80_DISP_RAM_SIZE = 0x4000;
    uint8_t _screenBuffer[ROBSZ80_DISP_RAM_SIZE];
    bool _screenBufferValid;

    static McDescriptorTable _descriptorTable;

public:

    McRobsZ80() : McBase()
    {
        _screenBufferValid = false;
    }

    // Enable machine
    virtual void enable(int subType);

    // Disable machine
    virtual void disable();

    // Get descriptor table for the machine (-1 for current subType)
    virtual McDescriptorTable* getDescriptorTable([[maybe_unused]] int subType = -1)
    {
        return &McRobsZ80::_descriptorTable;
    }

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefresh(DisplayBase* pDisplay);

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Handle a file
    virtual void fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal);

};
