// Bus Raider TRS80 Support
// Rob Dobson 2018

#include "mc_trs80.h"
#include "busraider.h"
#include "ee_printf.h"
#include "nmalloc.h"
#include "usb_hid_keys.h"
#include "wgfx.h"
#include "mc_trs80_cmdfile.h"
#include "target_memory_map.h"
#include "utils.h"
#include "rdutils.h"

extern WgfxFont __TRS80Level3Font;

#define TRS80_KEYBOARD_ADDR 0x3800
#define TRS80_KEYBOARD_RAM_SIZE 0x0100
#define TRS80_DISP_RAM_ADDR 0x3c00
#define TRS80_DISP_RAM_SIZE 0x400

static uint8_t __trs80ScreenBuffer[TRS80_DISP_RAM_SIZE];
static bool __trs80ScreenBufferValid = false;
static uint8_t __trs80KeyBuffer[TRS80_KEYBOARD_RAM_SIZE];
static bool __trs80KeyBufferDirty = false;

static void trs80_init()
{
    // Clear keyboard buffer
    for (int i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++)
        __trs80KeyBuffer[i] = 0;
    // Ensure keyboard is cleared initially
    __trs80KeyBufferDirty = true;
}

static void trs80_deinit()
{
}

static void trs80_keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // LogWrite(FromTRS80, 4, "Key %02x %02x", ucModifiers, rawKeys[0]);

    // TRS80 keyboard is mapped as follows
    // Addr	Bit 0 		1 		2 		3 		4 		5 		6 		7
    // 3801 	@ 		A 		B 		C 		D 		E 		F 		G
    // 3802 	H 		I 		J 		K 		L 		M 		N 		O
    // 3804 	P 		Q 		R 		S 		T 		U 		V 		W
    // 3808 	X 		Y 		Z
    // 3810 	0 		1 		2 		3 		4 		5 		6 		7
    // 3820 	8 		9 		* 		+ 		< 		= 		> 		?
    // 3840 	Enter 	Clear 	Break 	Up 		Down 	Left 	Right 	Space
    // 3880 	Shift 	***** 					Control

    static const int TRS80_KEY_BYTES = 8;
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
    for (int i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++) {
        // Clear initially
        kbdMap[i] = 0;
        // Set all locations that would be set in real TRS80 due to
        // matrix operation of keyboard on address lines
        for (int j = 0; j < TRS80_KEY_BYTES; j++) {
            if (i & (1 << j))
                kbdMap[i] |= keybdBytes[j];
        }
        // Check for changes
        if (kbdMap[i] != __trs80KeyBuffer[i])
        {
            __trs80KeyBuffer[i] = kbdMap[i];
            __trs80KeyBufferDirty = true;
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

static void trs80_displayHandler()
{
    // Read memory of RC2014 at the location of the TRS80 memory
    // mapped screen
    unsigned char pScrnBuffer[TRS80_DISP_RAM_SIZE];
    br_read_block(TRS80_DISP_RAM_ADDR, pScrnBuffer, TRS80_DISP_RAM_SIZE, 1, 0);

    // Write to the display on the Pi Zero
    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 64; i++) {
            register int cellIdx = k * 64 + i;
            if (!__trs80ScreenBufferValid || (__trs80ScreenBuffer[cellIdx] != pScrnBuffer[cellIdx]))
            {
                wgfx_putc(0, i, k, pScrnBuffer[cellIdx]);
                __trs80ScreenBuffer[cellIdx] = pScrnBuffer[cellIdx];
            }
        }
    }
    __trs80ScreenBufferValid = true;

    // Check for key presses and send to the TRS80 if necessary
    if (__trs80KeyBufferDirty)
    {
        br_write_block(TRS80_KEYBOARD_ADDR, __trs80KeyBuffer, TRS80_KEYBOARD_RAM_SIZE, 1, 0);
        __trs80KeyBufferDirty = false;
    }
}

static void handleTrs80ExecAddr(uint32_t execAddr)
{
    // Handle the execution address
    uint8_t jumpCmd[3] = { 0xc3, execAddr & 0xff, (execAddr >> 8) & 0xff };
    targetDataBlockStore(0, jumpCmd, 3);
    LogWrite("TRS80", LOG_DEBUG, "Added JMP %04x at 0000\n", execAddr);
}

static void trs80_handle_file(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type
    #define MAX_VALUE_STR 30
    char fileType[MAX_VALUE_STR+1];
    if (!jsonGetValueForKey("fileType", pFileInfo, fileType, MAX_VALUE_STR))
        return;
    if (strcmp(fileType, "trs80cmd") == 0)
    {
        LogWrite("TRS80", LOG_DEBUG, "Processing TRS80 cmd file len %d\n", fileLen);
        mc_trs80_cmdfile_proc(targetDataBlockStore, handleTrs80ExecAddr, pFileData, fileLen);
    }
    else if (strcmp(fileType, "trs80bin") == 0)
    {
        uint16_t baseAddr = 0;
        char baseAddrStr[MAX_VALUE_STR+1];
        if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
            baseAddr = strtol(baseAddrStr, NULL, 16);
        LogWrite("TRS80", LOG_DEBUG, "Processing TRS80 binary file, baseAddr %04x len %d\n", baseAddr, fileLen);
        targetDataBlockStore(baseAddr, pFileData, fileLen);
    }
}

static McGenericDescriptor trs80_descr = {
    // Initialisation function
    .pInit = trs80_init,
    .pDeInit = trs80_deinit,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 8 * 64,
    .displayPixelsY = 24 * 16,
    .displayCellX = 8,
    .displayCellY = 24,
    .pFont = &__TRS80Level3Font,
    .displayForeground = WGFX_GREEN,
    .displayBackground = WGFX_BLACK,
    // Keyboard
    .pKeyHandler = trs80_keyHandler,
    .pDispHandler = trs80_displayHandler,
    .pFileHandler = trs80_handle_file
};

McGenericDescriptor* trs80_get_descriptor(int subType)
{
    subType = subType;
    return &trs80_descr;
}
