// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McBase.h"

class McRobsZ80 : public McBase
{
private:
    static constexpr uint32_t ROBSZ80_DISP_RAM_ADDR = 0x4000;
    static constexpr uint32_t ROBSZ80_DISP_RAM_SIZE = 0x4000;
    uint8_t _screenBuffer[ROBSZ80_DISP_RAM_SIZE];
    bool _screenBufferValid;

    static McVariantTable _machineDescriptorTables[];

public:

    McRobsZ80(McManager& mcManager);

    // Enable machine
    virtual void enableMachine() override;

    // Disable machine
    virtual void disableMachine() override;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void refreshDisplay() override;

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) override;

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, 
                    int fileLen, TargetProgrammer& targetProgrammer) override;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) override;

    // Bus action active callback
    virtual void busActionActiveCallback(BR_BUS_ACTION actionType, 
                    BR_BUS_ACTION_REASON reason, BR_RETURN_TYPE rslt) override;
    
private:
    virtual void updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen);
};
