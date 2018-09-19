// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "ee_printf.h"
#include "usb_hid_keys.h"

class McZXSpectrum : public McBase
{
  private:
    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_ADDR = 0x4000;
    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_SIZE = 0x1b00;
    uint8_t _screenBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _screenBufferValid;
    static constexpr uint32_t ZXSPECTRUM_PIXEL_RAM_SIZE = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_OFFSET = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_DATA_SIZE = 0x300;

    static McDescriptorTable _descriptorTable;

    static void handleExecAddr(uint32_t execAddr);

    static const int MAX_KEYS = 6;
    static unsigned char _curKeyModifiers;
    static unsigned char _curKeys[MAX_KEYS];

  public:

    McZXSpectrum() : McBase()
    {
        _screenBufferValid = false;
        for (int i = 0; i < MAX_KEYS; i++)
            _curKeys[i] = KEY_NONE;
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
    virtual void fileHander(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    static uint32_t memoryRequestCallback(uint32_t addr, uint32_t data, uint32_t flags);

  private:
    static uint32_t getKeyPressed(const int* keyCodes, int keyCodesLen);
};
