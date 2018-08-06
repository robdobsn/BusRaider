// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"
#include "wgfxfont.h"

typedef unsigned char WGFX_COL;

extern void wgfx_init(void* p_framebuffer, unsigned int width, unsigned int height, 
				unsigned int pitch, unsigned int size);
extern void wgfx_clear();

extern void wgfx_term_putstring(const char* str);

void wgfx_putc(int windowIdx, unsigned int row, unsigned int col, unsigned char ch);

extern void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
                int cellWidth, int cellHeight, int xPixScale, int yPixScale,
                 WgfxFont* pFont);
extern void wgfx_set_console_window(int winIdx);

extern void wgfx_set_bg(WGFX_COL col);
extern void wgfx_set_fg(WGFX_COL col);
