// Bus Raider TRS80 Support
// Rob Dobson 2018

#include "mc_trs80.h"
#include "busraider.h"
#include "ee_printf.h"
#include "nmalloc.h"
#include "usb_hid_keys.h"
#include "wgfx.h"

extern WgfxFont __TRS80Level3Font;

#define TRS80_KEYBOARD_ADDR 0x3800
#define TRS80_KEYBOARD_RAM_SIZE 0x0100
#define TRS80_DISP_RAM_ADDR 0x3c00
#define TRS80_DISP_RAM_SIZE 0x400

static uint8_t* __trs80ScreenBuffer = NULL;

static void trs80_init()
{
    // Allocate storage for display
    __trs80ScreenBuffer = nmalloc_malloc(TRS80_DISP_RAM_SIZE);
}

static void trs80_deinit()
{
    // Allocate storage for display
    if (__trs80ScreenBuffer)
        nmalloc_free((void*)__trs80ScreenBuffer);
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
        kbdMap[i] = 0;
        for (int j = 0; j < TRS80_KEY_BYTES; j++) {
            if (i & (1 << j))
                kbdMap[i] |= keybdBytes[j];
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

    // Write to target machine RAM
    br_write_block(TRS80_KEYBOARD_ADDR, kbdMap, TRS80_KEYBOARD_RAM_SIZE, 1, 0);
}

static void trs80_displayHandler()
{
    // LogWrite(FromTRS80, 4, ".");
    unsigned char pScrnBuffer[0x400];
    br_read_block(0x3c00, pScrnBuffer, 0x400, 1, 0);
    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 64; i++) {
            wgfx_putc(0, i, k, pScrnBuffer[k * 64 + i]);
        }
    }
}

static McGenericDescriptor trs80_descr = {
    // Initialisation function
    .pInit = trs80_init,
    .pDeInit = trs80_deinit,
    // Required display refresh rate
    .displayRefreshRatePerSec = 25,
    .displayPixelsX = 8 * 64,
    .displayPixelsY = 24 * 16,
    .displayCellX = 8,
    .displayCellY = 24,
    .pFont = &__TRS80Level3Font,
    // Keyboard
    .pKeyHandler = trs80_keyHandler,
    .pDispHandler = trs80_displayHandler
};

McGenericDescriptor* trs80_get_descriptor(int subType)
{
    subType = subType;
    return &trs80_descr;
}
