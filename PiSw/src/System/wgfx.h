// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "wgfxfont.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern WgfxFont __systemFont;

typedef unsigned char DISPLAY_FX_COL;

// enum DISPLAY_FX_COLR_NAMES
// {
// 	DISPLAY_FX_BLACK,
// 	DISPLAY_FX_DARK_RED,
// 	DISPLAY_FX_DARK_GREEN,
// 	DISPLAY_FX_DARK_YELLOW,
// 	DISPLAY_FX_DARK_BLUE,
// 	DISPLAY_FX_DARK_PURPLE,
// 	DISPLAY_FX_DARK_CYAN,
// 	DISPLAY_FX_GRAY,
// 	DISPLAY_FX_DARK_GRAY,
// 	DISPLAY_FX_RED,
// 	DISPLAY_FX_GREEN,
// 	DISPLAY_FX_YELLOW,
// 	DISPLAY_FX_BLUE,
// 	DISPLAY_FX_PURPLE,
// 	DISPLAY_FX_CYAN,
// 	DISPLAY_FX_WHITE
// };

extern void wgfx_init(unsigned int desiredWidth, unsigned int desiredHeight);
extern void wgfx_set_framebuffer(void* p_framebuffer, unsigned int width, unsigned int height,
    unsigned int pitch, unsigned int size);
extern void wgfx_clear();

extern void wgfx_console_putchar(char ch);
extern void wgfx_console_putstring(const char* str);
extern int wgfx_get_console_width();

extern void wgfx_puts(int winIdx, unsigned int col, unsigned int row, const uint8_t* pStr);
extern void wgfx_putc(int windowIdx, unsigned int col, unsigned int row, unsigned char ch);

extern void wgfxSetMonoPixel(int windowIdx, int x, int y, int value);
extern void wgfxSetColourPixel(int winIdx, int x, int y, int colour);

extern void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont, int foregroundColour, int backgroundColour,
    int borderWidth, int borderColour);

extern void wgfx_set_console_window(int winIdx);

extern void wgfx_scroll(int winIdx, int rows);

extern void wgfx_set_bg(DISPLAY_FX_COL col);
extern void wgfx_set_fg(DISPLAY_FX_COL col);

extern void wgfx_wait_for_prev_operation();

extern void wgfx_term_render_cursor();
extern void wgfx_restore_cursor_content();
extern unsigned char* wgfx_get_win_pfb(int winIdx, int col, int row);
extern unsigned char* wgfx_get_win_pfb_xy(int winIdx, int x, int y);
extern void wgfxHLine(int x, int y, int len, int colour);
extern void wgfxVLine(int x, int y, int len, int colour);

#ifdef __cplusplus
}
#endif
