// Bus Raider Machine TRS80
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "TargetRegisters.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TRS80
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class McTRS80 : public McBase
{
public:

    McTRS80(McManager& mcManager, BusControl& busControl);

    // Enable machine
    virtual void enableMachine() override;

    // Disable machine
    virtual void disableMachine() override;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void refreshDisplay() override;

    // Machine heartbeat
    virtual void machineHeartbeat() override;

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
    // Helpers
    void updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen);
    void handleWD1771DiskController(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

private:
    // TRS80 memory map
    static constexpr uint32_t TRS80_KEYBOARD_ADDR = 0x3800;
    static constexpr uint32_t TRS80_KEYBOARD_RAM_SIZE = 0x0100;
    static constexpr uint32_t TRS80_DISP_RAM_ADDR = 0x3c00;
    static constexpr uint32_t TRS80_DISP_RAM_SIZE = 0x400;

    // Buffer for screen content
    uint8_t _screenBuffer[TRS80_DISP_RAM_SIZE];
    bool _screenBufferValid;

    // Buffer for Keyboard mapping
    uint8_t _keyBuffer[TRS80_KEYBOARD_RAM_SIZE];
    bool _keyBufferDirty;

    // Variants of this machine
    static McVariantTable _machineDescriptorTables[];
};
