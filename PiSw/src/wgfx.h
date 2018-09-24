// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"
#include "wgfxfont.h"

#ifdef __cplusplus
extern "C" {
#endif

extern WgfxFont __systemFont;

typedef unsigned char WGFX_COL;

enum WGFX_COLR_NAMES
{
	WGFX_BLACK,
	WGFX_DARK_RED,
	WGFX_DARK_GREEN,
	WGFX_DARK_YELLOW,
	WGFX_DARK_BLUE,
	WGFX_DARK_PURPLE,
	WGFX_DARK_CYAN,
	WGFX_GRAY,
	WGFX_DARK_GRAY,
	WGFX_RED,
	WGFX_GREEN,
	WGFX_YELLOW,
	WGFX_BLUE,
	WGFX_PURPLE,
	WGFX_CYAN,
	WGFX_WHITE
};

extern void wgfx_init(unsigned int desiredWidth, unsigned int desiredHeight);
extern void wgfx_set_framebuffer(void* p_framebuffer, unsigned int width, unsigned int height,
    unsigned int pitch, unsigned int size);
extern void wgfx_clear();

extern void wgfx_term_putstring(const char* str);
extern int wgfx_get_term_width();

extern void wgfx_puts(int winIdx, unsigned int col, unsigned int row, uint8_t* pStr);
extern void wgfx_putc(int windowIdx, unsigned int col, unsigned int row, unsigned char ch);

extern void wgfxSetMonoPixel(int windowIdx, int x, int y, int value);
extern void wgfxSetColourPixel(int winIdx, int x, int y, int colour);

extern void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont, int foregroundColour, int backgroundColour,
    int borderWidth, int borderColour);

extern void wgfx_set_console_window(int winIdx);

extern void wgfx_scroll(int winIdx, int rows);

extern void wgfx_set_bg(WGFX_COL col);
extern void wgfx_set_fg(WGFX_COL col);

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
