// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"
#include "wgfxfont.h"

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

extern void wgfx_init(void* p_framebuffer, unsigned int width, unsigned int height,
    unsigned int pitch, unsigned int size);
extern void wgfx_clear();

extern void wgfx_term_putstring(const char* str);

void wgfx_putc(int windowIdx, unsigned int col, unsigned int row, unsigned char ch);

extern void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont, int foregroundColour, int backgroundColour,
    int borderWidth, int borderColour);
extern void wgfx_set_console_window(int winIdx);

extern void wgfx_scroll(int winIdx, int rows);

extern void wgfx_set_bg(WGFX_COL col);
extern void wgfx_set_fg(WGFX_COL col);
