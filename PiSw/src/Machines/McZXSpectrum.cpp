// Bus Raider Machine ZXSpectrum
// Rob Dobson 2018

#include <stdlib.h>
#include "McZXSpectrum.h"
#include "usb_hid_keys.h"
#include "../System/rdutils.h"
#include "../System/lowlib.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../Machines/McManager.h"
#include "../Hardware/HwManager.h"
#include "../FileFormats/McZXSpectrumTZXFormat.h"
#include "../FileFormats/McZXSpectrumSNAFormat.h"
#include "../FileFormats/McZXSpectrumZ80Format.h"
#include "../Fonts/SystemFont.h"

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

const char* McZXSpectrum::_logPrefix = "ZXSpectrum";

uint8_t McZXSpectrum::_spectrumKeyboardIOBitMap[ZXSPECTRUM_KEYBOARD_NUM_ROWS];

McDescriptorTable McZXSpectrum::_defaultDescriptorTables[] = {
    {
        // Machine name
        "ZX Spectrum",
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 50,
        .displayPixelsX = 256,
        .displayPixelsY = 192,
        .displayCellX = 8,
        .displayCellY = 8,
        .pixelScaleX = 4,
        .pixelScaleY = 4,
        .pFont = &__systemFont,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = true,
        // Clock
        .clockFrequencyHz = 3500000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = true,
        .monitorMREQ = false,
        .setRegistersCodeAddr = ZXSPECTRUM_DISP_RAM_ADDR
    }
};

McZXSpectrum::McZXSpectrum() : McBase(_defaultDescriptorTables, sizeof(_defaultDescriptorTables)/sizeof(_defaultDescriptorTables[0]))
{
    // Screen needs redrawing
    _screenCacheValid = false;

    // Clear key bitmap
    for (int i = 0; i < ZXSPECTRUM_KEYBOARD_NUM_ROWS; i++)
        _spectrumKeyboardIOBitMap[i] = 0xff;
}

// Enable machine
void McZXSpectrum::enable()
{
    _screenCacheValid = false;
}

// Disable machine
void McZXSpectrum::disable()
{
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McZXSpectrum::displayRefreshFromMirrorHw()
{
    // Read mirror memory at the location of the memory mapped screen
    unsigned char pScrnBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
        // static int maxddd = 0;
        // if (maxddd++ == 100)
        // {
        //     maxddd = 0;
    if (HwManager::blockRead(ZXSPECTRUM_DISP_RAM_ADDR, pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE, 1, 0, true) == BR_OK)
    {
            // LogWrite(_logPrefix, LOG_DEBUG, "DISP REF %d %02x %02x", pScrnBuffer, pScrnBuffer[0x1800], pScrnBuffer[0x1801]);
        updateDisplayFromBuffer(pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE);
    }
        // }
}

void McZXSpectrum::updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen)
{    
    if (!_pDisplay || (bufLen < ZXSPECTRUM_DISP_RAM_SIZE))
        return;

    // Colour lookup table
    static const int NUM_SPECTRUM_COLOURS = 16;
    static DISPLAY_FX_COLOUR colourLUT[NUM_SPECTRUM_COLOURS] = {
            DISPLAY_FX_BLACK,
            DISPLAY_FX_DARK_BLUE,
            DISPLAY_FX_DARK_RED,
            DISPLAY_FX_DARK_PURPLE,
            DISPLAY_FX_DARK_GREEN,
            DISPLAY_FX_DARK_CYAN,
            DISPLAY_FX_DARK_YELLOW,
            DISPLAY_FX_GRAY,
            DISPLAY_FX_BLACK,
            DISPLAY_FX_BLUE,
            DISPLAY_FX_RED,
            DISPLAY_FX_PURPLE,
            DISPLAY_FX_GREEN,
            DISPLAY_FX_CYAN,
            DISPLAY_FX_YELLOW,
            DISPLAY_FX_WHITE
    };

    // Check for X scale == 4 (fast implementation if so)
    if (_activeDescriptorTable.pixelScaleX == 4)
    {
        // Get the raw screen access
        FrameBufferInfo fbi;
        _pDisplay->getFramebuffer(fbi);

        // Work through pixel and colour data together
        int colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE;
        uint8_t* pFB = fbi.pFBWindow;
        int pitch = fbi.pitch;
        int pitchDiv4 = fbi.pitch / 4;
        int scaleX = _activeDescriptorTable.pixelScaleX;
        int scaleY = _activeDescriptorTable.pixelScaleY;
        int SPECTRUM_CELL_STRIDE = _activeDescriptorTable.displayPixelsX / _activeDescriptorTable.displayCellX;
        int SPECTRUM_LINE_STRIDE = _activeDescriptorTable.displayCellY * _activeDescriptorTable.displayPixelsX / _activeDescriptorTable.displayCellX;

        // Iterate down cells
        for (int cellY = 0; cellY < _activeDescriptorTable.displayPixelsY/_activeDescriptorTable.displayCellY; cellY++)
        {
            // Framebuffer for this row of cells
            int pfbIdx = cellY * _activeDescriptorTable.displayCellY * scaleY * pitch;
            
            // Iterate cells across
            for (int cellX = 0; cellX < _activeDescriptorTable.displayPixelsX/_activeDescriptorTable.displayCellX; cellX++)
            {
                // Colours are the same for all pixels in cell
                int cellColourData = pScrnBuffer[colrIdx];
                DISPLAY_FX_COLOUR paperColour = colourLUT[((cellColourData & 0x38) >> 3) | ((cellColourData & 0x40) >> 3)];
                DISPLAY_FX_COLOUR inkColour = colourLUT[(cellColourData & 0x07) | ((cellColourData & 0x40) >> 3)];

                // Lines of cell
                int pixByteIdx = cellX + (cellY & 0x07) * SPECTRUM_CELL_STRIDE + (cellY & 0x18) * SPECTRUM_LINE_STRIDE;
                for (int cellPixY = 0; cellPixY < _activeDescriptorTable.displayCellY; cellPixY++)
                {
                    if ((_screenCache[colrIdx] != pScrnBuffer[colrIdx]) || (_screenCache[pixByteIdx] != pScrnBuffer[pixByteIdx]))
                    {
                        // Pixels
                        int pixMask = 0x80;
                        uint32_t* pBufCell = (uint32_t*) (pFB + pfbIdx + (cellPixY * scaleY) * pitch);
                        for (int pixIdx = 0; pixIdx < 8; pixIdx++)
                        {
                            bool pixVal = pScrnBuffer[pixByteIdx] & pixMask;
                            int pixColour = pixVal ? inkColour : paperColour;
                            uint32_t pixColourL = (pixColour << 24) + (pixColour << 16) + (pixColour << 8) + pixColour;
                            uint32_t* pBufL = pBufCell + pixIdx;
                            for (int iy = 0; iy < scaleY; iy++)
                            {
                                *pBufL = pixColourL;
                                pBufL += pitchDiv4;
                            }
                            // Bump the pixel mask
                            pixMask = pixMask >> 1;
                        }

                        // Bump pixel position
                        pixByteIdx += SPECTRUM_LINE_STRIDE;

                        // Update pixel cache
                        _screenCache[pixByteIdx] = pScrnBuffer[pixByteIdx];
                    }
                }

                // Framebuffer position update - start of next cell
                pfbIdx += _activeDescriptorTable.displayCellX * scaleX;

                // Update colour cache
                _screenCache[colrIdx] = pScrnBuffer[colrIdx];

                // Update colour idx
                colrIdx++;
            }
        }
    }
    else
    {
        // Check for colour data change - refresh everything if changed
        for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
        {
            if (pScrnBuffer[colrIdx] != _screenCache[colrIdx])
            {
                _screenCacheValid = false;
                break;
            }
        }

        // Write to the display on the Pi Zero
        int bytesPerRow = _activeDescriptorTable.displayPixelsX/8;
        int colrCellsPerRow = _activeDescriptorTable.displayPixelsX/_activeDescriptorTable.displayCellX;
        for (uint32_t bufIdx = 0; bufIdx < ZXSPECTRUM_PIXEL_RAM_SIZE; bufIdx++)
        {
            if (!_screenCacheValid || (_screenCache[bufIdx] != pScrnBuffer[bufIdx]))
            {
                // Save new version of screen buffer
                _screenCache[bufIdx] = pScrnBuffer[bufIdx];

                // Get colour info for this location
                int col = bufIdx % colrCellsPerRow;
                int row = (bufIdx / colrCellsPerRow);
                row = (row & 0x07) | ((row & 0xc0) >> 3);
                int colrAddr = ZXSPECTRUM_PIXEL_RAM_SIZE + (row*colrCellsPerRow) + col;

                // Bits 0..2 are INK colour, 3..5 are PAPER colour, 6 = brightness, 7 = flash
                int cellColourData = pScrnBuffer[colrAddr];

                // Set the pixels in this byte
                int pixMask = 0x80;
                for (int i = 0; i < 8; i++)
                {
                    int x = ((bufIdx % bytesPerRow) * 8) + i;
                    int y = bufIdx / bytesPerRow;
                    // Munge y value for weird spectrum addressing
                    y = ((y & 0x38) >> 3) | ((y & 0x07) << 3) | (y & 0xc0);
                    // Get pixel colour
                    bool pixVal = pScrnBuffer[bufIdx] & pixMask;
                    DISPLAY_FX_COLOUR pixColour = colourLUT[((cellColourData & 0x38) >> 3) | ((cellColourData & 0x40) >> 3)];
                    if (pixVal)
                        pixColour = colourLUT[(cellColourData & 0x07) | ((cellColourData & 0x40) >> 3)]; 
                    _pDisplay->setPixel(x, y, 1, pixColour);
                    // Bump the pixel mask
                    pixMask = pixMask >> 1;
                }
            }
        }

        // Save colour data for later checks
        for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
            _screenCache[colrIdx] = pScrnBuffer[colrIdx];
        _screenCacheValid = true;
    }
}

uint32_t McZXSpectrum::getKeyBitmap(const int* keyCodes, int keyCodesLen, const uint8_t currentKeyPresses[MAX_KEYS])
{
    uint32_t retVal = 0xff;
    for (int i = 0; i < MAX_KEYS; i++)
    {
        int bitMask = 0x01;
        for (int j = 0; j < keyCodesLen; j++)
        {
            if (currentKeyPresses[i] == keyCodes[j])
                retVal &= ~bitMask;
            bitMask = bitMask << 1;
        }
    }
    return retVal;
}

// Handle a key press
void McZXSpectrum::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[MAX_KEYS])
{
    // Clear key bitmap
    for (int i = 0; i < ZXSPECTRUM_KEYBOARD_NUM_ROWS; i++)
        _spectrumKeyboardIOBitMap[i] = 0xff;

    // Note that in the following I've used the KEY_HANJA as a placeholder
    // as I think it is a key that won't normally occur

    // Check for special codes in the key buffer
    bool specialKeyBackspace = false;
    for (int i = 0; i < MAX_KEYS; i++)
    {
        if (rawKeys[i] == KEY_BACKSPACE)
            specialKeyBackspace = true;
    }

    // Key table
    static const int keyTable[ZXSPECTRUM_KEYBOARD_NUM_ROWS][ZXSPECTRUM_KEYS_IN_ROW] = {
            {KEY_HANJA, KEY_Z, KEY_X, KEY_C, KEY_V},
            {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G},
            {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T},
            {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5},
            {KEY_0, KEY_9, KEY_8, KEY_7, KEY_6},
            {KEY_P, KEY_O, KEY_I, KEY_U, KEY_Y},
            {KEY_ENTER, KEY_L, KEY_K, KEY_J, KEY_H},
            {KEY_SPACE, KEY_HANJA, KEY_M, KEY_N, KEY_B}
        };

    // Handle encoding of keys
    for (int keyRow = 0; keyRow < ZXSPECTRUM_KEYBOARD_NUM_ROWS; keyRow++)
    {
        uint32_t keyBits = getKeyBitmap(keyTable[keyRow], ZXSPECTRUM_KEYS_IN_ROW, rawKeys);
        _spectrumKeyboardIOBitMap[keyRow] = keyBits;
    }

    // Handle shift key modifier (inject a shift for backspace)
    if (specialKeyBackspace || ((ucModifiers & KEY_MOD_LSHIFT) != 0) || ((ucModifiers & KEY_MOD_RSHIFT) != 0))
        _spectrumKeyboardIOBitMap[0] &= 0xfe;

    // Handle modifier for delete (on the zero key)
    if (specialKeyBackspace)
        _spectrumKeyboardIOBitMap[4] &= 0xfe;

    // Handle Sym key (CTRL)
    if (((ucModifiers & KEY_MOD_LCTRL) != 0) || ((ucModifiers & KEY_MOD_RCTRL) != 0))
        _spectrumKeyboardIOBitMap[7] &= 0xfd;

    // LogWrite(_logPrefix, LOG_DEBUG, "KeyBits %02x %02x %02x %02x %02x %02x %02x %02x", 
    //                 _spectrumKeyboardIOBitMap[0],
    //                 _spectrumKeyboardIOBitMap[1],
    //                 _spectrumKeyboardIOBitMap[2],
    //                 _spectrumKeyboardIOBitMap[3],
    //                 _spectrumKeyboardIOBitMap[4],
    //                 _spectrumKeyboardIOBitMap[5],
    //                 _spectrumKeyboardIOBitMap[6],
    //                 _spectrumKeyboardIOBitMap[7]
    //                 );

}

// Check if machine can process a file type
bool McZXSpectrum::canProcFileType(const char* pFileType)
{
    if (strcasecmp(pFileType, "tzx") == 0)
        return true;
    if (strcasecmp(pFileType, "z80") == 0)
        return true;
    if (strcasecmp(pFileType, "sna") == 0)
        return true;
    return false;
}

// Handle a file
bool McZXSpectrum::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_FILE_NAME_STR 100
    char fileName[MAX_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_FILE_NAME_STR))
        return false;

    // Get type of file (assume extension is delimited by .)
    const char* pFileType = strstr(fileName, ".");
    const char* pEmpty = "";
    if (pFileType == NULL)
        pFileType = pEmpty;

    // See if it is TZX format
    if (strcasecmp(pFileType, ".tzx") == 0)
    {
        // TZX
        McZXSpectrumTZXFormat formatHandler;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing TZX file len %d", fileLen);
        formatHandler.proc(TargetState::addMemoryBlock, TargetState::setTargetRegisters, pFileData, fileLen);
    }
    else if (strcasecmp(pFileType, ".z80") == 0)
    {
        // .Z80
        McZXSpectrumZ80Format formatHandler;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing Z80 file len %d", fileLen);
        // Handle registers and injecting RET
        formatHandler.proc(TargetState::addMemoryBlock, TargetState::setTargetRegisters, pFileData, fileLen);
    }
    else if (strcasecmp(pFileType, ".sna") == 0)
    {
        // SNA
        McZXSpectrumSNAFormat formatHandler;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing SNA file len %d", fileLen);
        // Handle the format
        formatHandler.proc(TargetState::addMemoryBlock, TargetState::setTargetRegisters, pFileData, fileLen);
    }
    else
    {
        // Treat everything else as a binary file
        uint16_t baseAddr = 0;
        char baseAddrStr[MAX_VALUE_STR+1];
        if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
            baseAddr = strtol(baseAddrStr, NULL, 16);
        LogWrite(_logPrefix, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
        TargetState::addMemoryBlock(baseAddr, pFileData, fileLen);
    }
    return true;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void McZXSpectrum::busAccessCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
    
    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    #endif

    if ((flags & BR_CTRL_BUS_RD_MASK) && (flags & BR_CTRL_BUS_IORQ_MASK))
    {
        // Check for a keyboard address range - any even port number
        if ((addr & 0x01) == 0)
        {
            // Iterate bits in upper address to get the code by and-ing the key bits
            // this emulates the operation of a bitmapped keyboard matrix
            retVal = 0xff;
            uint32_t addrBitMask = 0x0100;
            for (int keyRow = 0; keyRow < ZXSPECTRUM_KEYBOARD_NUM_ROWS; keyRow++)
            {
                if ((addr & addrBitMask) == 0)
                    retVal &= _spectrumKeyboardIOBitMap[keyRow];
                addrBitMask = addrBitMask << 1;
            }
        }
        else if ((addr & 0xff) == 0x1f)
        {
            // Kempston joystick - just say nothing pressed
            retVal = 0;
        }

    }

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    #endif
}

// Bus action complete callback
void McZXSpectrum::busActionCompleteCallback(BR_BUS_ACTION actionType)
{
    // Check for BUSRQ
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // Read memory at the location of the memory mapped screen
        unsigned char pScrnBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
        if (BusAccess::blockRead(ZXSPECTRUM_DISP_RAM_ADDR, pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE, 1, 0) == BR_OK)
            updateDisplayFromBuffer(pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE);

        // // Generate a maskable interrupt to trigger Spectrum's keyboard ISR
        McManager::targetIrq();
    }
}
