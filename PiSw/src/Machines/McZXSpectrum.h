// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McBase.h"
#include "usb_hid_keys.h"
#include "../TargetBus/TargetRegisters.h"

class McZXSpectrum : public McBase
{
private:
    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_ADDR = 0x4000;
    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_SIZE = 0x1b00;
    uint8_t _screenBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _screenBufferValid;
    uint32_t _screenBufferRefreshY;
    uint32_t _screenBufferRefreshX;
    uint32_t _screenBufferRefreshCount;
    uint8_t _screenCache[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _screenCacheValid;

    uint8_t* _pFrameBuffer;
    uint32_t _pfbSize;
    uint32_t _framePitch;
    uint32_t _framePitchDiv4;
    uint32_t _scaleX;
    uint32_t _scaleY;
    uint32_t _lineStride;
    uint32_t _scaledStrideY;
    uint32_t _scaledStrideX;
    uint32_t _cellsX;
    uint32_t _cellsY;
    uint32_t _cellSizeY;
    uint32_t _cellSizeX;

    static constexpr uint32_t ZXSPECTRUM_PIXEL_RAM_SIZE = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_OFFSET = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_DATA_SIZE = 0x300;

    static constexpr int ZXSPECTRUM_KEYBOARD_NUM_ROWS = 8;
    static constexpr int ZXSPECTRUM_KEYS_IN_ROW = 5;

    static McDescriptorTable _defaultDescriptorTables[];

    static const int MAX_KEYS = 6;

    static uint8_t _spectrumKeyboardIOBitMap[ZXSPECTRUM_KEYBOARD_NUM_ROWS];

    static const char* _logPrefix;

public:

    McZXSpectrum();

    // Enable machine
    virtual void enable();

    // Disable machine
    virtual void disable();

    // Service
    virtual void service();

    // Machine heartbeat
    virtual void machineHeartbeat();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefreshFromMirrorHw();

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Check if machine can process a file type
    virtual bool canProcFileType(const char* fileType);

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Bus action complete callback
    virtual void busActionCompleteCallback(BR_BUS_ACTION actionType);

private:
    static uint32_t getKeyBitmap(const int* keyCodes, int keyCodesLen, const uint8_t currentKeyPresses[MAX_KEYS]);
    void updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen);
};
