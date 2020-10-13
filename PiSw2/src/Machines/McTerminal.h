// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "logging.h"
#include "KeyConversion.h"
#include "TermEmu.h"

class McTerminal : public McBase
{
public:

    McTerminal(McManager& mcManager, BusAccess& busAccess);

    // Enable machine
    virtual void enableMachine() override;

    // Disable machine
    virtual void disableMachine() override;

    // Setup machine from JSON
    virtual bool setupMachine(const char* mcName, const char* mcJson) override;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void refreshDisplay() override;

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) override;

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, 
                    int fileLen, TargetProgrammer& targetProgrammer) override;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) override;

    // Bus action complete callback
    virtual void busActionCompleteCallback(BR_BUS_ACTION actionType) override;

    // Convert raw USB code to key string
    static const char* convertRawToKeyString(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Key conversion
    // TODO 2020
    // static uint32_t keyboardGetNumTypes() override
    // {
    //     return _keyConversion.getNumTypes();
    // }
    // static const char* keyboardGetTypeStr(uint32_t keyboardType) override
    // {
    //     return _keyConversion.getTypeStr(keyboardType);        
    // }
    // static void keyboardSetType(uint32_t keyboardType) override
    // {
    //     _keyConversion.setKeyboardType(keyboardType);
    // }

    // Get changes made since last mirror display update
    virtual uint32_t getMirrorChanges(uint8_t* pMirrorChangeBuf, uint32_t mirrorChangeMaxLen, bool forceGetAll) override;

private:
    TermChar _screenCache[TermEmu::MAX_ROWS * TermEmu::MAX_COLS];
    TermChar _screenMirrorCache[TermEmu::MAX_ROWS * TermEmu::MAX_COLS];
    uint32_t _cursorBlinkLastUs;
    uint32_t _cursorBlinkRateMs;
    bool _cursorIsShown;
    TermCursor _cursorInfo;

    // Emulated UARTS
    bool _emulate6850;
    bool _emulation6850NeedsReset;
    bool _emulation6850NotSetup;
    bool _emulationInterruptOnRx;

    static McVariantTable _machineDescriptorTables[];

    // Terminal emulation maintains an in-memory image of the screen
    TermEmu* _pTerminalEmulation;

    // Helpers
    void invalidateScreenCaches(bool mirrorOnly);

    // Buffer for chars to send to target
    static const int MAX_SEND_TO_TARGET_CHARS = 5000;
    RingBufferPosn _sendToTargetBufPos;
    uint8_t _sendToTargetBuf[MAX_SEND_TO_TARGET_CHARS];

    // Key conversion
    static KeyConversion _keyConversion;

};
