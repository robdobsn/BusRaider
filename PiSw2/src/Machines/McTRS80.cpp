// Bus Raider Machine TRS80
// Rob Dobson 2018-2020

#include <stdlib.h>
#include "McTRS80.h"
#include "usb_hid_keys.h"
#include "rdutils.h"
#include "BusControl.h"
#include "TargetProgrammer.h"
#include "McManager.h"
#include "McTRS80CmdFormat.h"
#include "DebugHelper.h"

static const char* MODULE_PREFIX = "TRS80";

// #define DEBUG_TRS80_KEYBOARD

extern WgfxFont __TRS80Level3Font;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine descriptors - describe the machine and variants
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

McVariantTable McTRS80::_machineDescriptorTables[] = {
    {
        // Machine name
        "TRS80",
        // Processor
        McVariantTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 50,
        .displayPixelsX = 8 * 64,
        .displayPixelsY = 24 * 16,
        .displayCellX = 8,
        .displayCellY = 24,
        .pixelScaleX = 2,
        .pixelScaleY = 2,
        .pFont = &__TRS80Level3Font,
        .displayForeground = DISPLAY_FX_GREEN,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = true,
        // Clock
        .clockFrequencyHz = 1770000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = true,
        .monitorMREQ = true,
        .setRegistersCodeAddr = TRS80_DISP_RAM_ADDR
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

McTRS80::McTRS80(McManager& mcManager, BusControl& busControl) : 
        McBase(mcManager, busControl, _machineDescriptorTables, 
                sizeof(_machineDescriptorTables)/sizeof(_machineDescriptorTables[0]))
{
    // Clear keyboard buffer
    for (uint32_t i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++)
        _keyBuffer[i] = 0;

    // Ensure keyboard is cleared initially
    _keyBufferDirty = true;

    // Screen buffer invalid
    _screenBufferValid = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable / Disable - called when selecting/deselecting a machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Enable machine
void McTRS80::enableMachine()
{
    // Invalidate screen buffer
    _screenBufferValid = false;
    _keyBufferDirty = false;
}

// Disable machine
void McTRS80::disableMachine()
{
}

// void McTRS80::handleRegisters(Z80Registers& regs)
// {
//     // Handle the execution address
//     uint32_t execAddr = regs.PC;
//     uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
//     TargetProgrammer::addMemoryBlock(0, jumpCmd, 3);
//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle display refresh (called at a rate indicated by the machine's descriptor table)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::refreshDisplay()
{
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "refreshDisplay");

    // Read memory at the location of the TRS80 memory mapped screen
    unsigned char pScrnBuffer[TRS80_DISP_RAM_SIZE];
    bool readOk = _busControl.mem().blockRead(TRS80_DISP_RAM_ADDR, pScrnBuffer, TRS80_DISP_RAM_SIZE, BLOCK_ACCESS_MEM) == BR_OK;
    if (readOk)
        updateDisplayFromBuffer(pScrnBuffer, TRS80_DISP_RAM_SIZE);

    // Check for key presses and send to the TRS80 if necessary
    if (_keyBufferDirty)
    {
#ifdef DEBUG_TRS80_KEYBOARD
        for (uint32_t j = 0; j < TRS80_KEYBOARD_RAM_SIZE; j++) 
        {
            if (_keyBuffer[j] != 0)
            {
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "First key non 0 at %04x == %02x",
                        TRS80_KEYBOARD_ADDR + j, _keyBuffer[j]);
                break;
            }
        }
#endif
        _busControl.mem().blockWrite(TRS80_KEYBOARD_ADDR, _keyBuffer, TRS80_KEYBOARD_RAM_SIZE, BLOCK_ACCESS_MEM);
        _keyBufferDirty = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Heartbeat called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::machineHeartbeat()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Display update helper
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen)
{
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "updateDisplayFromBuffer");

    DisplayBase* pDisplay = getDisplay();
    if (!pDisplay || (bufLen < TRS80_DISP_RAM_SIZE))
        return;

    // Write to the display on the Pi Zero
    int cols = getDescriptorTable().displayPixelsX / getDescriptorTable().displayCellX; 
    int rows = getDescriptorTable().displayPixelsY / getDescriptorTable().displayCellY;
    for (int k = 0; k < rows; k++) 
    {
        for (int i = 0; i < cols; i++)
        {
            int cellIdx = k * cols + i;
            if (!_screenBufferValid || (_screenBuffer[cellIdx] != pScrnBuffer[cellIdx]))
            {
                pDisplay->write(i, k, pScrnBuffer[cellIdx]);
                _screenBuffer[cellIdx] = pScrnBuffer[cellIdx];
            }
        }
    }
    _screenBufferValid = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Keypress helper
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
#ifdef DEBUG_TRS80_KEYBOARD
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "keyHandler %02x %02x", ucModifiers, rawKeys[0]);
#endif

    // TRS80 keyboard is mapped as follows
    // Addr Bit 0       1       2       3       4       5       6       7
    // 3801     @       A       B       C       D       E       F       G
    // 3802     H       I       J       K       L       M       N       O
    // 3804     P       Q       R       S       T       U       V       W
    // 3808     X       Y       Z
    // 3810     0       1       2       3       4       5       6       7
    // 3820     8       9       *       +       <       =       >       ?
    // 3840     Enter   Clear   Break   Up      Down    Left    Right   Space
    // 3880     Shift   *****                   Control

    uint8_t keybdBytes[TRS80_KEY_BYTES];
    for (int i = 0; i < TRS80_KEY_BYTES; i++)
        keybdBytes[i] = 0;

    // Go through key codes
    int suppressShift = 0;
    int suppressCtrl = 0;
    for (int keyIdx = 0; keyIdx < 6; keyIdx++) {
        // Key
        unsigned char rawKey = rawKeys[keyIdx];

        // Handle key
        if ((rawKey >= KEY_A) && (rawKey <= KEY_Z)) {
            // Handle A..Z
            int bitIdx = ((rawKey - KEY_A) + 1) % 8;
            keybdBytes[(((rawKey - KEY_A) + 1) / 8)] |= (1 << bitIdx);
        } else if ((rawKey == KEY_2) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle @
            keybdBytes[0] |= 1;
            suppressShift = 1;
        } else if ((rawKey == KEY_6) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle ^
            suppressShift = 1;
        } else if ((rawKey == KEY_7) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle &
            keybdBytes[4] |= 0x40;
        } else if ((rawKey == KEY_8) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle *
            keybdBytes[5] |= 4;
        } else if ((rawKey == KEY_9) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle (
            keybdBytes[5] |= 1;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_0) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle )
            keybdBytes[5] |= 2;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey >= KEY_1) && (rawKey <= KEY_9)) {
            // Handle 1..9
            int bitIdx = ((rawKey - KEY_1) + 1) % 8;
            keybdBytes[(((rawKey - KEY_1) + 1) / 8) + 4] |= (1 << bitIdx);
        } else if (rawKey == KEY_0) {
            // Handle 0
            keybdBytes[4] |= 1;
        } else if ((rawKey == KEY_SEMICOLON) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle ;
            keybdBytes[5] |= 8;
            suppressShift = 1;
        } else if ((rawKey == KEY_SEMICOLON) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle :
            keybdBytes[5] |= 4;
            suppressShift = 1;
        } else if ((rawKey == KEY_APOSTROPHE) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle "
            keybdBytes[4] |= 4;
            keybdBytes[7] |= 0x01;
        } else if (rawKey == KEY_COMMA) {
            // Handle <
            keybdBytes[5] |= 0x10;
        } else if (rawKey == KEY_DOT) {
            // Handle >
            keybdBytes[5] |= 0x40;
        } else if ((rawKey == KEY_EQUAL) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle =
            keybdBytes[5] |= 0x20;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_EQUAL) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle +
            keybdBytes[5] |= 0x8;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_MINUS) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle -
            keybdBytes[5] |= 0x20;
            suppressShift = 1;
        } else if (rawKey == KEY_SLASH) {
            // Handle ?
            keybdBytes[5] |= 0x80;
        } else if (rawKey == KEY_ENTER) {
            // Handle Enter
            keybdBytes[6] |= 0x01;
        } else if (rawKey == KEY_BACKSPACE) {
            // Treat as LEFT
            keybdBytes[6] |= 0x20;
        } else if (rawKey == KEY_ESC) {
            // Handle Break
            keybdBytes[6] |= 0x04;
        } else if (rawKey == KEY_UP) {
            // Handle Up
            keybdBytes[6] |= 0x08;
        } else if (rawKey == KEY_DOWN) {
            // Handle Down
            keybdBytes[6] |= 0x10;
        } else if (rawKey == KEY_LEFT) {
            // Handle Left
            keybdBytes[6] |= 0x20;
        } else if (rawKey == KEY_RIGHT) {
            // Handle Right
            keybdBytes[6] |= 0x40;
        } else if (rawKey == KEY_SPACE) {
            // Handle Space
            keybdBytes[6] |= 0x80;
        } else if (rawKey == KEY_F1) {
            // Handle CLEAR
            keybdBytes[6] |= 0x02;
        } else if (rawKey == KEY_LEFTSHIFT) {
            // Handle Left Shift
            keybdBytes[7] |= 0x01;
        } else if (rawKey == KEY_RIGHTSHIFT) {
            // Handle Left Shift
            keybdBytes[7] |= 0x02;
        } else if ((rawKey == KEY_LEFTCTRL) || (rawKey == KEY_RIGHTCTRL)) {
            // Handle <
            keybdBytes[7] |= 0x10;
        }
    }

    // Suppress shift keys if needed
    if (suppressShift) {
        keybdBytes[7] &= 0xfc;
    }
    if (suppressCtrl) {
        keybdBytes[7] &= 0xef;
    }

    // Build RAM map
    uint8_t kbdMap[TRS80_KEYBOARD_RAM_SIZE];
    for (uint32_t i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++) 
    {
        // Clear initially
        kbdMap[i] = 0;
        // Set all locations that would be set in real TRS80 due to
        // matrix operation of keyboard on address lines
        for (int j = 0; j < TRS80_KEY_BYTES; j++) 
        {
            if (i & (1 << j))
                kbdMap[i] |= keybdBytes[j];
        }
        // Check for changes
        if (kbdMap[i] != _keyBuffer[i])
        {
            _keyBuffer[i] = kbdMap[i];
            _keyBufferDirty = true;
        }
    }

    // DEBUG
    // for (int i = 0; i < 16; i++)
    // {
    //     uart_printf("%02x..", i*16);
    //     for (int j = 0; j < 16; j++)
    //     {
    //         uart_printf("%02x ", kbdMap[i*16+j]);
    //     }
    //     uart_printf("\n");
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McTRS80::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen,
                TargetProgrammer& targetProgrammer)
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "fileHandler %s", pFileInfo);

    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_FILE_NAME_STR 100
    char fileName[MAX_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_FILE_NAME_STR))
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "failed to get key fileName");
        return false;
    }
    // Check type of file (assume extension is delimited by .)
    const char* pFileType = strstr(fileName, ".");
    const char* pEmpty = "";
    if (pFileType == NULL)
        pFileType = pEmpty;
    if (strcasecmp(pFileType, ".cmd") == 0)
    {
        // TRS80 command file
        McTRS80CmdFormat cmdFormat;
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing TRS80 CMD file len %d", fileLen);
        cmdFormat.proc(targetProgrammer.addMemoryBlockStatic, 
                    targetProgrammer.setTargetRegistersStatic, 
                    &targetProgrammer, pFileData, fileLen);
    }
    else
    {
        // Treat everything else as a binary file
        uint16_t baseAddr = 0;
        char baseAddrStr[MAX_VALUE_STR+1];
        if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
            baseAddr = strtoul(baseAddrStr, NULL, 16);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
        targetProgrammer.addMemoryBlock(baseAddr, pFileData, fileLen);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::busAccessCallback( uint32_t addr,  uint32_t data, 
             uint32_t flags,  uint32_t& retVal)
{
    // if (flags & BR_CTRL_BUS_IORQ_MASK)
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "IORQ %s from %04x %02x", 
    //             (flags & BR_CTRL_BUS_RD_MASK) ? "RD" : ((flags & BR_CTRL_BUS_WR_MASK) ? "WR" : "??"),
    //             addr, 
    //             (flags & BR_CTRL_BUS_WR_MASK) ? data : retVal);

    // Check for read from IO
    if ((flags & BR_CTRL_BUS_RD_MASK) && (flags & BR_CTRL_BUS_IORQ_MASK))
    {
        // Decode port
        if ((addr & 0xff) == 0x13)  // Joystick
        {
            // Indicate no buttons are pressed
            retVal = 0xff;
        }
    }
    // Check for read from IO
    else if ((flags & BR_CTRL_BUS_RD_MASK) && (flags & BR_CTRL_BUS_IORQ_MASK))
    {
        // FDD
        if ((addr >= 0x37ec) && (addr < 0x37ef))
        {
            handleWD1771DiskController(addr-0x37ec, data, flags, retVal);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle WD1771 access - Floppy disk controller
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::handleWD1771DiskController( uint32_t addr,  uint32_t data, 
             uint32_t flags,  uint32_t& retVal)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus action active callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McTRS80::busActionActiveCallback(BR_BUS_ACTION actionType, 
                    BR_BUS_ACTION_REASON reason, BR_RETURN_TYPE rslt)
{
}
