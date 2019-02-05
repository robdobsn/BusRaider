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

    static void handleExecAddr(uint32_t execAddr);

  public:

    McRobsZ80() : McBase()
    {
        _screenBufferValid = false;
    }

    // Enable machine
    virtual void enable();

    // Disable machine
    virtual void disable();

    // Get descriptor table for the machine
    virtual McDescriptorTable* getDescriptorTable([[maybe_unused]] int subType)
    {
        return &_descriptorTable;
    }

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
