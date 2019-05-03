// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "../System/logging.h"

class McTerminal : public McBase
{
private:
    static const char* _logPrefix;
    static const int DEFAULT_TERM_COLS = 80;
    static const int DEFAULT_TERM_ROWS = 30;
    static const int MAX_TERM_COLS = 120;
    static const int MAX_TERM_ROWS = 50;
    int _termCols;
    int _termRows;
    uint8_t _screenChars[MAX_TERM_ROWS * MAX_TERM_COLS];
    uint8_t _screenBuffer[MAX_TERM_ROWS * MAX_TERM_COLS];
    bool _screenBufferValid;
    int _curPosX;
    int _curPosY;
    bool _cursorShow;
    uint32_t _cursorBlinkLastUs;
    uint32_t _cursorBlinkRateMs;
    bool _cursorIsOn;
    uint8_t _cursorChar;

    static McDescriptorTable _defaultDescriptorTables[];

    // Shifted digits on keyboard
    static const int SHIFT_DIGIT_KEY_MAP_LEN = 10;
    static int _shiftDigitKeyMap[SHIFT_DIGIT_KEY_MAP_LEN];

public:

    McTerminal();

    // Enable machine
    virtual void enable();

    // Disable machine
    virtual void disable();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefreshFromMirrorHw();

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal);

    // Bus action complete callback
    virtual void busActionCompleteCallback(BR_BUS_ACTION actionType);

    // Convert raw USB code to ASCII
    static int convertRawToAscii(unsigned char ucModifiers, const unsigned char rawKeys[6]);

private:
    void clearScreen();
    void dispChar(uint8_t ch, DisplayBase* pDisplay);
    void vscrollBuffer(int rows);
    void moveAndCheckCurPos(int absX, int absY, int relX, int relY, DisplayBase* pDisplay);
};
