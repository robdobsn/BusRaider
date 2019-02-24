// Bus Raider
// Rob Dobson 2019

#include "Display.h"
#include "wgfx.h"
#include "ee_printf.h"
#include <string.h>

Display::Display()
{

}

Display::~Display()
{

}

bool Display::init(int displayWidth, int displayHeight)
{
    wgfx_init(displayWidth, displayHeight);
    return true;
}

void Display::targetLayout(int tlX, int tlY,
                int pixX, int pixY, 
                int cellX, int cellY, 
                int xScale, int yScale,
                WgfxFont* pFont, 
                int foreColour, int backColour, 
                int borderWidth, int borderColour)
{
    // Clear display
    wgfx_clear();
    
    // Layout display
    wgfx_set_window(0, tlX, tlY, pixX, pixY, cellX, cellY, xScale, yScale,
                pFont, foreColour, backColour, borderWidth, borderColour);
    wgfx_set_window(1, 0, (pixY*yScale) + borderWidth*2 + 10, 
        -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        0, 8);
    wgfx_set_console_window(1);
}

void Display::targetSetChar()
{

}

void Display::windowWrite(int winIdx, int col, int row, const uint8_t* pStr)
{
    wgfx_puts(winIdx, col, row, pStr);
}

void Display::termWrite(const char* pStr)
{
    wgfx_term_putstring(pStr);
}

void Display::termWrite(int ch)
{
    wgfx_term_putchar(ch);
}

void Display::termColour(int colour)
{
    wgfx_set_fg(colour);
}

int Display::termGetWidth()
{
    return wgfx_get_term_width();
}
