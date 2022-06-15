// Bus Raider Machine ZXSpectrum
// Rob Dobson 2018-2020

#include <stdlib.h>
#include "McZXSpectrum.h"
#include "usb_hid_keys.h"
#include "rdutils.h"
#include "lowlib.h"
#include "BusControl.h"
#include "TargetImager.h"
#include "McManager.h"
#include "McZXSpectrumTZXFormat.h"
#include "McZXSpectrumSNAFormat.h"
#include "McZXSpectrumZ80Format.h"
#include "SystemFont.h"
#include "PiWiring.h"
#include "DebugHelper.h"
#include "DisplayChange.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern WgfxFont __pZXSpectrumFont;

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

static const char* MODULE_PREFIX = "ZXSpectrum";

McVariantTable McZXSpectrum::_machineDescriptorTables[] = {
    {
        // Machine name
        "ZX Spectrum",
        McVariantTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 50,
        .displayPixelsX = 256,
        .displayPixelsY = 192,
        .displayCellX = 8,
        .displayCellY = 8,
        .pixelScaleX = 4,
        .pixelScaleY = 4,
        .pFont = &__pZXSpectrumFont,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = true,
        // Clock
        .clockFrequencyHz = 3500000,
        // Interrupt rate in T-States (clock cycles)
        .irqRate = 69888,
        // Bus monitor
        .monitorIORQ = true,
        .monitorMREQ = false,
        .setRegistersCodeAddr = ZXSPECTRUM_DISP_RAM_ADDR
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

McZXSpectrum::McZXSpectrum(McManager& mcManager, BusControl& busControl) : 
            McBase(mcManager, busControl, _machineDescriptorTables, 
            sizeof(_machineDescriptorTables)/sizeof(_machineDescriptorTables[0]))
{
    // Screen needs redrawing
    _screenBufferValid = false;
    _screenCacheValid = false;
    _screenBufferRefreshY = 0;
    _screenBufferRefreshX = 0;
    _screenBufferRefreshCount = 0;
    _pFrameBuffer = NULL;
    _pfbSize = 0;
    _cellsY = _machineDescriptorTables[0].displayPixelsY/_machineDescriptorTables[0].displayPixelsY;
    _cellsX = _machineDescriptorTables[0].displayPixelsX/_machineDescriptorTables[0].displayPixelsX;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable/Disable 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Enable machine
void McZXSpectrum::enableMachine()
{
    _screenBufferValid = false;
    _screenRefreshRequired = false;
    _screenCacheValid = false;
    _mirrorCacheValid = false;
    _screenBufferRefreshY = 0;
    _screenBufferRefreshX = 0;
    _screenBufferRefreshCount = 0;
    _pFrameBuffer = NULL;
    _pfbSize = 0;
}

// Disable machine
void McZXSpectrum::disableMachine()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Heartbeat/service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McZXSpectrum::machineHeartbeat()
{
    // Generate a maskable interrupt to trigger Spectrum's ISR
    _mcManager.targetIrq();
}

void McZXSpectrum::service()
{
    // Check if display data valid
    if (_screenRefreshRequired)
    {
        _screenBufferRefreshCount = 0;
        _screenRefreshRequired = false;
    }

    // Check if refresh completed
    if (_screenBufferRefreshCount != _cellsY * _cellsX)
    {
        updateDisplayFromBuffer(_screenBuffer, ZXSPECTRUM_DISP_RAM_SIZE);
        _screenBufferRefreshCount++;
    }

    // Check for keys in queue
    if (_keyBuf.keyChangeReady())
    {
        // Get keys to current bitmap
        _keyBuf.getToCurBitMap();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Display Refresh
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McZXSpectrum::refreshDisplay()
{
    // Read mirror memory at the location of the memory mapped screen
    if (_busControl.mem().blockRead(ZXSPECTRUM_DISP_RAM_ADDR, _screenBuffer, ZXSPECTRUM_DISP_RAM_SIZE, BLOCK_ACCESS_MEM) == BR_OK)
    {
        _screenBufferValid = true;
        _screenRefreshRequired = true;
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "DISP REF %d %02x %02x", _screenBuffer, _screenBuffer[0x1800], _screenBuffer[0x1801]);
    }
}

void McZXSpectrum::updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen)
{    
    DisplayBase* pDisplay = getDisplay();
    if (!pDisplay || (bufLen < ZXSPECTRUM_DISP_RAM_SIZE))
        return;

    // Colour lookup table
    static const int NUM_SPECTRUM_COLOURS = 16;
    static const DISPLAY_FX_COLOUR colourLUT[NUM_SPECTRUM_COLOURS] = {
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
    if (getDescriptorTable().pixelScaleX == 4)
    {
        // Check valid frame buffer
        if (!_pFrameBuffer)
        {
            // Get the raw screen access
            FrameBufferInfo fbi;
            pDisplay->getFrameBufferInfo(fbi);
            _pFrameBuffer = fbi.pFBWindow;
            _pfbSize = fbi.pixelsWidth * fbi.pixelsHeight * fbi.bytesPerPixel;
            _framePitch = fbi.pitch;
            _framePitchDiv4 = fbi.pitch / 4;
            _scaleX = getDescriptorTable().pixelScaleX;
            _scaleY = getDescriptorTable().pixelScaleY;
            _lineStride = getDescriptorTable().displayCellY * getDescriptorTable().displayPixelsX / getDescriptorTable().displayCellX;
            _cellSizeY = getDescriptorTable().displayCellY;
            _cellSizeX = getDescriptorTable().displayCellX;
            _scaledStrideY = getDescriptorTable().displayCellY * _scaleY * _framePitch;
            _scaledStrideX = _cellSizeX * _scaleX;
            _cellsX = getDescriptorTable().displayPixelsX / getDescriptorTable().displayCellX;
            _cellsY = getDescriptorTable().displayPixelsY / getDescriptorTable().displayCellY;
        }

        // Cell index
        uint32_t cellX = _screenBufferRefreshX++;
        uint32_t cellY = _screenBufferRefreshY;
        if (_screenBufferRefreshX >= _cellsX)
        {
            _screenBufferRefreshX = 0;
            _screenBufferRefreshY++;
            if (_screenBufferRefreshY >= _cellsY)
            {
                _screenBufferRefreshY = 0;
            }
        }

        // Work through pixel and colour data together
        uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE + cellY * _cellsX + cellX;

        // Framebuffer for this row of cells
        uint32_t pfbIdx = cellY * _scaledStrideY + cellX * _scaledStrideX;

        // Colours are the same for all pixels in cell
        int cellColourData = pScrnBuffer[colrIdx];
        bool cellInvalid = (!_screenCacheValid) || (cellColourData != _screenCache[colrIdx]);

        if ((_screenBufferRefreshX == 31) && (_screenBufferRefreshY == 23))
            cellInvalid = true;

        if (cellInvalid)
        {
            if (colrIdx < ZXSPECTRUM_PIXEL_RAM_SIZE + ZXSPECTRUM_COLOUR_DATA_SIZE)
            {
                // Update colour cache
                _screenCache[colrIdx] = pScrnBuffer[colrIdx];
            }
            // else
            // {
            //     if (!pixDisp)
            //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "colrIdx out of bounds %d > %d", colrIdx, ZXSPECTRUM_PIXEL_RAM_SIZE + ZXSPECTRUM_COLOUR_DATA_SIZE);
            //     pixDisp = true;
            //     return;
            // }
        }

        // Lines of cell
        // ZXSpectrum lines are not in sequential order!
        uint32_t pixByteIdx = cellX + (cellY & 0x07) * _cellsX + (cellY & 0x18) * _lineStride;

        // // Validate
        // if (pixByteIdx >= ZXSPECTRUM_PIXEL_RAM_SIZE + ZXSPECTRUM_COLOUR_DATA_SIZE)
        // {
        //     if (!pixDisp)
        //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "pixByteIdx out of bounds %d > %d", pixByteIdx, ZXSPECTRUM_PIXEL_RAM_SIZE + ZXSPECTRUM_COLOUR_DATA_SIZE);
        //     pixDisp = true;
        //     return;
        // }

        // Check for cell pixel change
        uint32_t pixByteCheckIdx = pixByteIdx;
        for (uint32_t cellPixY = 0; cellPixY < _cellSizeY; cellPixY++)
        {
            if ((_screenCache[pixByteCheckIdx] != pScrnBuffer[pixByteCheckIdx]) && (pixByteCheckIdx < ZXSPECTRUM_PIXEL_RAM_SIZE))
            {
                _screenCache[pixByteCheckIdx] = pScrnBuffer[pixByteCheckIdx];
                cellInvalid = true;
            }
            pixByteCheckIdx += _lineStride;
        }

        // If anything invalid
        if (cellInvalid)
        {
            DISPLAY_FX_COLOUR paperColour = colourLUT[((cellColourData & 0x38) >> 3) | ((cellColourData & 0x40) >> 3)];
            DISPLAY_FX_COLOUR inkColour = colourLUT[(cellColourData & 0x07) | ((cellColourData & 0x40) >> 3)];
            for (uint32_t cellPixY = 0; cellPixY < _cellSizeY; cellPixY++)
            {
                // Pixels
                uint32_t pixMask = 0x80;
                uint32_t* pBufCell = (uint32_t*) (_pFrameBuffer + pfbIdx + (cellPixY * _scaleY) * _framePitch);
                for (uint32_t pixIdx = 0; pixIdx < 8; pixIdx++)
                {
                    bool pixVal = pScrnBuffer[pixByteIdx] & pixMask;
                    uint32_t pixColour = pixVal ? inkColour : paperColour;
                    uint32_t pixColourL = (pixColour << 24) + (pixColour << 16) + (pixColour << 8) + pixColour;
                    uint32_t* pBufL = pBufCell + pixIdx;
                    if ((uint8_t*)(pBufL + _framePitchDiv4 * _scaleY) < _pFrameBuffer + _pfbSize)
                    {
                        for (uint32_t iy = 0; iy < _scaleY; iy++)
                        {
                            *pBufL = pixColourL;
                            pBufL += _framePitchDiv4;
                        }
                    }
                    // Bump the pixel mask
                    pixMask = pixMask >> 1;
                }

                // Bump pixel position
                pixByteIdx += _lineStride;
            }
        }

        if ((_screenBufferRefreshX == 0) && (_screenBufferRefreshY == 0))
            _screenCacheValid = true;
    }
    // else
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "PIXSCALEX %d", getDescriptorTable().pixelScaleX);
    //     microsDelay(10000);

    //     return;

    //     // Check for colour data change - refresh everything if changed
    //     for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
    //     {
    //         if (pScrnBuffer[colrIdx] != _screenCache[colrIdx])
    //         {
    //             _screenCacheValid = false;
    //             break;
    //         }
    //     }

    //     // Write to the display on the Pi Zero
    //     int bytesPerRow = getDescriptorTable().displayPixelsX/8;
    //     int colrCellsPerRow = getDescriptorTable().displayPixelsX/getDescriptorTable().displayCellX;
    //     for (uint32_t bufIdx = 0; bufIdx < ZXSPECTRUM_PIXEL_RAM_SIZE; bufIdx++)
    //     {
    //         if (!_screenCacheValid || (_screenCache[bufIdx] != pScrnBuffer[bufIdx]))
    //         {
    //             // Save new version of screen buffer
    //             _screenCache[bufIdx] = pScrnBuffer[bufIdx];

    //             // Get colour info for this location
    //             int col = bufIdx % colrCellsPerRow;
    //             int row = (bufIdx / colrCellsPerRow);
    //             row = (row & 0x07) | ((row & 0xc0) >> 3);
    //             int colrAddr = ZXSPECTRUM_PIXEL_RAM_SIZE + (row*colrCellsPerRow) + col;

    //             // Bits 0..2 are INK colour, 3..5 are PAPER colour, 6 = brightness, 7 = flash
    //             int cellColourData = pScrnBuffer[colrAddr];

    //             // Set the pixels in this byte
    //             int pixMask = 0x80;
    //             for (int i = 0; i < 8; i++)
    //             {
    //                 int x = ((bufIdx % bytesPerRow) * 8) + i;
    //                 int y = bufIdx / bytesPerRow;
    //                 // Munge y value for weird spectrum addressing
    //                 y = ((y & 0x38) >> 3) | ((y & 0x07) << 3) | (y & 0xc0);
    //                 // Get pixel colour
    //                 bool pixVal = pScrnBuffer[bufIdx] & pixMask;
    //                 DISPLAY_FX_COLOUR pixColour = colourLUT[((cellColourData & 0x38) >> 3) | ((cellColourData & 0x40) >> 3)];
    //                 if (pixVal)
    //                     pixColour = colourLUT[(cellColourData & 0x07) | ((cellColourData & 0x40) >> 3)]; 
    //                 pDisplay->setPixel(x, y, 1, pixColour);
    //                 // Bump the pixel mask
    //                 pixMask = pixMask >> 1;
    //             }
    //         }
    //     }

    //     // Save colour data for later checks
    //     for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
    //         _screenCache[colrIdx] = pScrnBuffer[colrIdx];
    //     _screenCacheValid = true;
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t McZXSpectrum::getKeyBitmap(const int* keyCodes, int keyCodesLen, 
            const uint8_t* currentKeyPresses)
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle a key press
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McZXSpectrum::keyHandler( unsigned char ucModifiers, 
                 const unsigned char* rawKeys)
{
    // Note that in the following I've used the KEY_HANJA as a placeholder
    // as I think it is a key that won't normally occur

    // Clear key buf scratchpad 
    _keyBuf.clearScratch();

    // Check shift & ctrl
    bool isShift = ((ucModifiers & KEY_MOD_LSHIFT) != 0) || ((ucModifiers & KEY_MOD_RSHIFT) != 0);
    bool isCtrl = ((ucModifiers & KEY_MOD_LCTRL) != 0) || ((ucModifiers & KEY_MOD_RCTRL) != 0);

    // Regular keys mapped to Spectrum keys
    struct KeyCodeDef {
        uint8_t regularKeyCode;
        bool isShifted;
        uint8_t matrixIdx;
        uint8_t matrixCode;
        uint8_t shiftCode;
    };
    static const KeyCodeDef regularKeyMapping[] = {
        {KEY_EQUAL, false, 6, 0xfd, 0xfd},
        {KEY_EQUAL, true, 6, 0xfb, 0xfd},
    };
    
    // Check for regular key codes in the key buffer
    bool specialKeyBackspace = false;
    for (int i = 0; i < MAX_KEYS; i++)
    {
        if (rawKeys[i] == KEY_BACKSPACE)
            specialKeyBackspace = true;
        
        // Check regular keys
        for (unsigned j = 0; j < sizeof(regularKeyMapping)/sizeof(KeyCodeDef); j++)
        {
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "check regular mapping %d isShift %d code %02x",
            //                     j, isShift, regularKeyMapping[j].regularKeyCode);
            if ((rawKeys[i] == regularKeyMapping[j].regularKeyCode) && (regularKeyMapping[j].isShifted == isShift))
            {
                _keyBuf.andScratch(regularKeyMapping[j].matrixIdx, regularKeyMapping[j].matrixCode);
                _keyBuf.andScratch(ZXSPECTRUM_SHIFT_KEYS_ROW_IDX, regularKeyMapping[j].shiftCode);
            }
        }
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
        _keyBuf.andScratch(keyRow, keyBits);
    }

    // Handle CAPS-SHIFT (shift on std keyboard) key modifier (inject one for backspace)
    if (specialKeyBackspace || isShift)
        _keyBuf.andScratch(ZXSPECTRUM_BS_KEY_ROW_IDX, 0xfe);

    // Handle modifier for delete (on the zero key)
    if (specialKeyBackspace)
        _keyBuf.andScratch(ZXSPECTRUM_DEL_KEY_ROW_IDX, 0xfe);

    // Handle Sym key (ctrl)
    if (isCtrl)
        _keyBuf.andScratch(ZXSPECTRUM_SHIFT_KEYS_ROW_IDX, 0xfd);

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "keyHandler mod %02x Key[0] %02x Key[1] %02x KeyBits %02x %02x %02x %02x %02x %02x %02x %02x", 
    //                 ucModifiers, rawKeys[0], rawKeys[1],
    //                 _keyBuf.getScratchBitMap(0),
    //                 _keyBuf.getScratchBitMap(1),
    //                 _keyBuf.getScratchBitMap(2),
    //                 _keyBuf.getScratchBitMap(3),
    //                 _keyBuf.getScratchBitMap(4),
    //                 _keyBuf.getScratchBitMap(5),
    //                 _keyBuf.getScratchBitMap(6),
    //                 _keyBuf.getScratchBitMap(7)
    //                 );

    // Add to keyboard queue
    _keyBuf.putFromScratch();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
bool McZXSpectrum::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen,
                TargetImager& targetImager)
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
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing TZX file len %d", fileLen);
        formatHandler.proc(targetImager.addMemoryBlockStatic, 
                targetImager.setCPURegistersStatic,
                &targetImager,
                pFileData, fileLen);
    }
    else if (strcasecmp(pFileType, ".z80") == 0)
    {
        // .Z80
        McZXSpectrumZ80Format formatHandler;
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing Z80 file len %d", fileLen);
        // Handle registers and injecting RET
        formatHandler.proc(targetImager.addMemoryBlockStatic, 
                targetImager.setCPURegistersStatic,
                &targetImager,
                pFileData, fileLen);
    }
    else if (strcasecmp(pFileType, ".sna") == 0)
    {
        // SNA
        McZXSpectrumSNAFormat formatHandler;
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing SNA file len %d", fileLen);
        // Handle the format
        formatHandler.proc(targetImager.addMemoryBlockStatic, 
                targetImager.setCPURegistersStatic,
                &targetImager,
                pFileData, fileLen);
    }
    else
    {
        // Treat everything else as a binary file
        uint16_t baseAddr = 0;
        char baseAddrStr[MAX_VALUE_STR+1];
        if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
            baseAddr = strtoul(baseAddrStr, NULL, 16);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
        targetImager.addMemoryBlock(baseAddr, pFileData, fileLen);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void McZXSpectrum::busAccessCallback( uint32_t addr,  uint32_t data, 
             uint32_t flags,  uint32_t& retVal)
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
                    retVal &= _keyBuf.getCurBitMap(keyRow);
                addrBitMask = addrBitMask << 1;
            }
        }
        else if ((addr & 0xff) == 0x1f)
        {
            // Kempston joystick - just say nothing pressed
            retVal = 0;
        }
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IO Read from %04x flags %04x data %02x retVal %02x", addr, flags, data, retVal);

    }

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    #endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus actions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Bus action active callback
void McZXSpectrum::busReqAckedCallback(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    // // Check for BUSRQ
    // if ((actionType == BR_BUS_ACTION_BUSRQ) && (rslt == BR_OK))
    // {
    //     // Read memory at the location of the memory mapped screen
    //     if (_busControl.blockRead(ZXSPECTRUM_DISP_RAM_ADDR, _screenBuffer, ZXSPECTRUM_DISP_RAM_SIZE, BLOCK_ACCESS_MEM) == BR_OK)
    //         _screenBufferValid = true;

    //     // // TODO
    //     // // Read FRAMES1, FRAMES2 & FRAMES3 ZX Spectrum variables and display on screen
    //     // static const int VARS_BUF_LEN = 0x3;
    //     // uint8_t varsBuf[VARS_BUF_LEN];
    //     // if (BusAccess::blockRead(0x5c78, varsBuf, VARS_BUF_LEN, false, false) == BR_OK)
    //     // {
    //     //     char buf2[100];
    //     //     buf2[0] = 0;
    //     //     int lineIdx = 0;
    //     //     for (uint32_t i = 0; i < VARS_BUF_LEN; i++)
    //     //     {
    //     //         char buf1[10];
    //     //         ee_sprintf(buf1, "%02x ", varsBuf[i]);
    //     //         strlcat(buf2, buf1, 100);
    //     //         if (i % 0x10 == 0x0f)
    //     //         {
    //     //             pDisplay->write(0, lineIdx, buf2);
    //     //             lineIdx++;
    //     //             buf2[0] = 0;
    //     //         }
    //     //     }
    //     //     if (strlen(buf2) > 0)
    //     //         pDisplay->write(0, lineIdx, buf2);
    //     // }

    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get changes to screen contents
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t McZXSpectrum::getMirrorChanges(uint8_t* pMirrorChangeBuf, uint32_t mirrorChangeMaxLen, bool forceGetAll)
{
    // TODO implement differences-only protocol?

    // Check buffer valid
    if (!_screenBufferValid)
        return 0;

    // Check for changes or forced get
    bool forceUpdate = forceGetAll || (!_mirrorCacheValid);
    if (!forceUpdate)
        forceUpdate = memcmp(_screenBuffer, _mirrorCache, ZXSPECTRUM_DISP_RAM_SIZE) != 0;

    // Check if update needed
    if (!forceUpdate)
        return 0;

    // Update cache
    memcpy(_mirrorCache, _screenBuffer, ZXSPECTRUM_DISP_RAM_SIZE);
    _mirrorCacheValid = true;

    // Screen info
    int cols = getDescriptorTable().displayPixelsX; 
    int rows = getDescriptorTable().displayPixelsY;

    // Calculate how much to send
    uint32_t bytesToSend = ZXSPECTRUM_DISP_RAM_SIZE - _nextMirrorPos;
    if (bytesToSend > MAX_BYTES_IN_EACH_MIRROR_UPDATE)
        bytesToSend = MAX_BYTES_IN_EACH_MIRROR_UPDATE;

    // Init response buffer for full screen dump
    uint32_t bufPos = DisplayChange::initResponse(
                pMirrorChangeBuf, 
                mirrorChangeMaxLen, 
                DisplayChange::FULL_SCREEN_UPDATE, 
                cols, 
                rows, 
                DisplayChange::PIXEL_BASED_SCREEN, 
                DisplayChange::SPECTRUM_SCREEN_LAYOUT,
                _nextMirrorPos,
                ZXSPECTRUM_DISP_RAM_SIZE);

    // Copy screen to buffer
    memcpy(pMirrorChangeBuf+bufPos, _screenBuffer + _nextMirrorPos, bytesToSend);

    // Calculate place to send from next time
    if (_nextMirrorPos + MAX_BYTES_IN_EACH_MIRROR_UPDATE > ZXSPECTRUM_DISP_RAM_SIZE)
        _nextMirrorPos = 0;
    else
        _nextMirrorPos += MAX_BYTES_IN_EACH_MIRROR_UPDATE;

    // Return length to send
    return bytesToSend + bufPos;
}
