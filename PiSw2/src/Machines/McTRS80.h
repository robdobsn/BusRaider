// Bus Raider Machine TRS80
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "CPUHandler_Z80Regs.h"

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
                int fileLen, TargetImager& targetImager) override;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) override;

    // Bus action active callback
    virtual void busReqAckedCallback(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt) override;

    // Get changes made since last mirror display update
    virtual uint32_t getMirrorChanges(uint8_t* pMirrorChangeBuf, uint32_t mirrorChangeMaxLen, bool forceGetAll) override;

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

    // Screen cache
    uint8_t _mirrorCache[TRS80_DISP_RAM_SIZE];
    bool _mirrorCacheValid = false;

    // Buffer for Keyboard mapping
    static const int TRS80_KEY_BYTES = 8;
    uint8_t _keyBuffer[TRS80_KEYBOARD_RAM_SIZE];
    bool _keyBufferDirty;

    // Variants of this machine
    static McVariantTable _machineDescriptorTables[];
};
