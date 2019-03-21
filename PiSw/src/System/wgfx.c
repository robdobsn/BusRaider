// Pi Bus Raider
// Rob Dobson 2018

#include "wgfx.h"
#include "../System/ee_printf.h"
#include "framebuffer.h"
#include "lowlib.h"
#include "lowlev.h"
#include "dma.h"
#include <stdbool.h>
#include <stddef.h>

extern WgfxFont __systemFont;

#define MIN(v1, v2) (((v1) < (v2)) ? (v1) : (v2))
#define MAX(v1, v2) (((v1) > (v2)) ? (v1) : (v2))

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window definition
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct WgfxWindowDef {
    int tlx;
    int width;
    int tly;
    int height;
    int cellWidth;
    int cellHeight;
    int xPixScale;
    int yPixScale;
    int foregroundColour;
    int backgroundColour;
    WgfxFont* pFont;
} WgfxWindowDef;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DISPLAY_FX_MAX_WINDOWS 5
static WgfxWindowDef __wgfxWindows[DISPLAY_FX_MAX_WINDOWS];
static bool __wgfxWindowValid[DISPLAY_FX_MAX_WINDOWS];
static bool __wgfxDMAActive = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Display Context
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned int screenWidth;
    unsigned int screenHeight;
    unsigned int pitch;
    unsigned int size;
    unsigned char* pfb;

    struct
    {
        unsigned int numCols;
        unsigned int numRows;
        unsigned int cursor_row;
        unsigned int cursor_col;
        unsigned int saved_cursor[2];
        char cursor_visible;
        int outputWinIdx;
    } term;

    DISPLAY_FX_COL bg;
    DISPLAY_FX_COL fg;

    // Make sure this is big enough for any font's character cell
    unsigned char cursor_buffer[512];

} DISPLAY_FX_CONTEXT;

static DISPLAY_FX_CONTEXT __displayContext;
unsigned int __attribute__((aligned(0x100))) mem_buff_dma[16];


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_init(unsigned int desiredWidth, unsigned int desiredHeight)
{
    microsDelay(10000);
    fb_release();

    unsigned char* p_fb = 0;
    unsigned int fbsize;
    unsigned int pitch;

    unsigned int p_w = desiredWidth;
    unsigned int p_h = desiredHeight;
    unsigned int v_w = p_w;
    unsigned int v_h = p_h;

    fb_init(p_w, p_h, v_w, v_h, 8, (void*)&p_fb, &fbsize, &pitch);

    fb_set_xterm_palette();

    if (fb_get_physical_buffer_size(&p_w, &p_h) != FB_SUCCESS) {
        // uart_printf("fb_get_physical_buffer_size error\n");
    }
    // uart_printf("physical fb size %dx%d\n", p_w, p_h);

    microsDelay(10000);
    wgfx_set_framebuffer(p_fb, v_w, v_h, pitch, fbsize);
    wgfx_clear();

    // Reset window validity
    for (int i = 0; i < DISPLAY_FX_MAX_WINDOWS; i++)
        __wgfxWindowValid[i] = false;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup framebuffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_set_framebuffer(void* p_framebuffer, unsigned int width, unsigned int height,
    unsigned int pitch, unsigned int size)
{
    dma_init();

    __displayContext.pfb = p_framebuffer;
    __displayContext.screenWidth = width;
    __displayContext.screenHeight = height;
    __displayContext.pitch = pitch;
    __displayContext.size = size;

    // Windows
    wgfx_set_window(0, 0, 0, width, height, -1, -1, 2, 2, NULL, -1, -1, 0, 0);

    // Initial settings
    __displayContext.term.numCols = __displayContext.screenWidth / __systemFont.cellX;
    __displayContext.term.numRows = __displayContext.screenHeight / __systemFont.cellY;
    __displayContext.term.cursor_row = __displayContext.term.cursor_col = 0;
    __displayContext.term.cursor_visible = 1;
    __displayContext.term.outputWinIdx = 0;

    __displayContext.bg = 0;
    __displayContext.fg = 15;
    wgfx_term_render_cursor();
}

void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont, int foregroundColour, int backgroundColour, 
    int borderWidth, int borderColour)
{
    WgfxFont* pFontToUse = (pFont != NULL) ? pFont : (&__systemFont);
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    // Width and height
    if (width == -1 && tlx != -1)
        __wgfxWindows[winIdx].width = __displayContext.screenWidth - tlx - borderWidth * 2;
    else if (width == -1)
        __wgfxWindows[winIdx].width = __displayContext.screenWidth;
    else
        __wgfxWindows[winIdx].width = width * xPixScale;
    if (height == -1)
        __wgfxWindows[winIdx].height = __displayContext.screenHeight - tly - borderWidth * 2;
    else
        __wgfxWindows[winIdx].height = height * yPixScale;
    // Top,Left
    if (tlx != -1)
        __wgfxWindows[winIdx].tlx = tlx * xPixScale + borderWidth;
    else
        __wgfxWindows[winIdx].tlx = (__displayContext.screenWidth - __wgfxWindows[winIdx].width) / 2;
    __wgfxWindows[winIdx].tly = tly * yPixScale + borderWidth;

    // Cell
    if (cellWidth == -1)
        __wgfxWindows[winIdx].cellWidth = pFontToUse->cellX;
    else
        __wgfxWindows[winIdx].cellWidth = cellWidth;
    if (cellHeight == -1)
        __wgfxWindows[winIdx].cellHeight = pFontToUse->cellY;
    else
        __wgfxWindows[winIdx].cellHeight = cellHeight;

    // Scale
    __wgfxWindows[winIdx].xPixScale = xPixScale;
    __wgfxWindows[winIdx].yPixScale = yPixScale;
    
    // Font
    __wgfxWindows[winIdx].pFont = pFontToUse;
    __wgfxWindows[winIdx].foregroundColour = foregroundColour;
    __wgfxWindows[winIdx].backgroundColour = backgroundColour;

    // Border
    if (borderColour != -1 && borderWidth > 0)
    {
        for (int i = 0; i < borderWidth; i++)
        {
            wgfxHLine(__wgfxWindows[winIdx].tlx-borderWidth, 
                    __wgfxWindows[winIdx].tly-borderWidth+i, 
                    __wgfxWindows[winIdx].width + borderWidth * 2,
                    borderColour);
            wgfxHLine(__wgfxWindows[winIdx].tlx-borderWidth, 
                    __wgfxWindows[winIdx].tly+__wgfxWindows[winIdx].height+i, 
                    __wgfxWindows[winIdx].width + borderWidth * 2,
                    borderColour);
            wgfxVLine(__wgfxWindows[winIdx].tlx-borderWidth+i, 
                    __wgfxWindows[winIdx].tly-borderWidth, 
                    __wgfxWindows[winIdx].height + borderWidth * 2,
                    borderColour);
            wgfxVLine(__wgfxWindows[winIdx].tlx+__wgfxWindows[winIdx].width+i, 
                    __wgfxWindows[winIdx].tly-borderWidth, 
                    __wgfxWindows[winIdx].height + borderWidth * 2,
                    borderColour);
        }
    }

    // uart_printf("idx %d, cx %d cy %d sx %d sy %d *pFont %02x %02x %02x\n\r\n",
    //     winIdx,
    //     __wgfxWindows[winIdx].pFont->cellX,
    //     __wgfxWindows[winIdx].pFont->cellY,
    //     __wgfxWindows[winIdx].xPixScale,
    //     __wgfxWindows[winIdx].yPixScale,
    //     pFontToUse->pFontData[0],
    //     pFontToUse->pFontData[1],
    //     pFontToUse->pFontData[2]);

    __wgfxWindowValid[winIdx] = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set window used for console
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_set_console_window(int winIdx)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    __displayContext.term.outputWinIdx = winIdx;
    // Reset term params
    __displayContext.term.numCols = __wgfxWindows[winIdx].width / (__wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale);
    __displayContext.term.numRows = __wgfxWindows[winIdx].height / (__wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale);
    __displayContext.term.cursor_row = __displayContext.term.cursor_col = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear entire display
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_clear()
{
    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();
    unsigned char* pf = __displayContext.pfb;
    unsigned char* pfb_end = pf + __displayContext.size;
    while (pf < pfb_end)
        *pf++ = __displayContext.bg;
}

void wgfx_clear_screen()
{
    wgfx_clear();
    wgfx_term_render_cursor();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cursor handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_check_curpos()
{
    if (__displayContext.term.cursor_col >= __displayContext.term.numCols) {
        __displayContext.term.cursor_row++;
        __displayContext.term.cursor_col = 0;
    }

    if (__displayContext.term.cursor_row >= __displayContext.term.numRows) {
        --__displayContext.term.cursor_row;
        wgfx_scroll(__displayContext.term.outputWinIdx, 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put to console window
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_console_putchar(char ch)
{
    switch (ch) {
    case '\r':
        __displayContext.term.cursor_col = 0;
        break;

    case '\n':
        __displayContext.term.cursor_col = 0;
        __displayContext.term.cursor_row++;
        wgfx_check_curpos();
        break;

    case 0x09: /* tab */
        __displayContext.term.cursor_col += 1;
        __displayContext.term.cursor_col = MIN(__displayContext.term.cursor_col + 8 - __displayContext.term.cursor_col % 8, __displayContext.term.numCols - 1);
        wgfx_check_curpos();
        break;

    case 0x08:
        /* backspace */
        if (__displayContext.term.cursor_col > 0) {
            __displayContext.term.cursor_col--;
            wgfx_putc(__displayContext.term.outputWinIdx, __displayContext.term.cursor_col, __displayContext.term.cursor_row, ' ');
        }
        break;

    default:
        wgfx_putc(__displayContext.term.outputWinIdx, __displayContext.term.cursor_col, __displayContext.term.cursor_row, ch);
        __displayContext.term.cursor_col++;
        wgfx_check_curpos();
        break;
    }
}

void wgfx_console_putstring(const char* str)
{
    wgfx_restore_cursor_content();
    while (*str)
        wgfx_console_putchar(*str++);
    wgfx_term_render_cursor();
}

int wgfx_get_console_width()
{
    return __displayContext.term.numCols;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put string to window
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_puts(int winIdx, unsigned int col, unsigned int row, const uint8_t* pStr)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    while(*pStr)
    {
        if ((int)col >= __wgfxWindows[winIdx].width / (__wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale))
            break;
        wgfx_putc(winIdx, col++, row, *pStr++);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put char to window
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfx_putc(int winIdx, unsigned int col, unsigned int row, unsigned char ch)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (!__wgfxWindowValid[winIdx])
        return;
    if ((int)col >= __wgfxWindows[winIdx].width / (__wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale))
        return;
    if ((int)row >= __wgfxWindows[winIdx].height / (__wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale))
        return;

    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(winIdx, col, row);
    // Pointer to font data to write into char cell
    unsigned char* pFont = __wgfxWindows[winIdx].pFont->pFontData + ch * __wgfxWindows[winIdx].pFont->bytesPerChar;

    // For each bit in the font character write the appropriate data to the pixel in framebuffer
    unsigned char* pBufCur = pBuf;
    int fgColour = ((__wgfxWindows[winIdx].foregroundColour != -1) ?
                    __wgfxWindows[winIdx].foregroundColour : __displayContext.fg);
    int bgColour = ((__wgfxWindows[winIdx].backgroundColour != -1) ?
                    __wgfxWindows[winIdx].backgroundColour : __displayContext.bg);
    int cellHeight = __wgfxWindows[winIdx].cellHeight;
    int yPixScale = __wgfxWindows[winIdx].yPixScale;
    int cellWidth = __wgfxWindows[winIdx].cellWidth;
    int xPixScale = __wgfxWindows[winIdx].xPixScale;
    for (int y = 0; y < cellHeight; y++) {
        for (int i = 0; i < yPixScale; i++) {
            pBufCur = pBuf;
            int bitMask = 0x01 << (cellWidth - 1);
            for (int x = 0; x < cellWidth; x++) {
                for (register int j = 0; j < xPixScale; j++) {
                    *pBufCur = (*pFont & bitMask) ? fgColour : bgColour;
                    pBufCur++;
                }
                bitMask = bitMask >> 1;
            }
            pBuf += __displayContext.pitch;
        }
        pFont += __wgfxWindows[winIdx].pFont->bytesAcross;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set Pixel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfxSetMonoPixel(int winIdx, int x, int y, int value)
{
    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();
    unsigned char* pBuf = wgfx_get_win_pfb_xy(winIdx, x, y);
    int fgColour = ((__wgfxWindows[winIdx].foregroundColour != -1) ?
                    __wgfxWindows[winIdx].foregroundColour : __displayContext.fg);
    int bgColour = ((__wgfxWindows[winIdx].backgroundColour != -1) ?
                    __wgfxWindows[winIdx].backgroundColour : __displayContext.bg);
    for (int iy = 0; iy < __wgfxWindows[winIdx].yPixScale; iy++)
    {
        unsigned char* pBufL = pBuf + iy * __displayContext.pitch;
        for (int ix = 0; ix < __wgfxWindows[winIdx].xPixScale; ix++)
            *pBufL++ = value ? fgColour : bgColour;
    }
}

void wgfxSetColourPixel(int winIdx, int x, int y, int colour)
{
    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();
    unsigned char* pBuf = wgfx_get_win_pfb_xy(winIdx, x, y);
    for (int iy = 0; iy < __wgfxWindows[winIdx].yPixScale; iy++)
    {
        unsigned char* pBufL = pBuf + iy * __displayContext.pitch;
        for (int ix = 0; ix < __wgfxWindows[winIdx].xPixScale; ix++)
            *pBufL++ = colour;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access a char cell
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Write data from a char cell in a window
void wgfx_write_cell(int winIdx, unsigned int col, unsigned int row, unsigned char* pCellBuf)
{
    if (col >= __displayContext.term.numCols)
        return;
    if (row >= __displayContext.term.numRows)
        return;
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (!__wgfxWindowValid[winIdx])
        return;

    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(winIdx, col, row);

    // Write data from cell buffer
    unsigned char* pBufCur = pBuf;
    for (int y = 0; y < __wgfxWindows[winIdx].cellHeight; y++) {
        for (int i = 0; i < __wgfxWindows[winIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < __wgfxWindows[winIdx].cellWidth; x++) {
                for (int j = 0; j < __wgfxWindows[winIdx].xPixScale; j++) {
                    *pBufCur++ = *pCellBuf++;
                }
            }
            pBuf += __displayContext.pitch;
        }
    }
}

// Read data from a char cell in a window
void wgfx_read_cell(int winIdx, unsigned int col, unsigned int row, unsigned char* pCellBuf)
{
    if (col >= __displayContext.term.numCols)
        return;
    if (row >= __displayContext.term.numRows)
        return;
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (!__wgfxWindowValid[winIdx])
        return;

    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(winIdx, col, row);

    // Write data from cell buffer
    unsigned char* pBufCur = pBuf;
    for (int y = 0; y < __wgfxWindows[winIdx].cellHeight; y++) {
        for (int i = 0; i < __wgfxWindows[winIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < __wgfxWindows[winIdx].cellWidth; x++) {
                for (int j = 0; j < __wgfxWindows[winIdx].xPixScale; j++) {
                    *pCellBuf++ = *pBufCur++;
                }
            }
            pBuf += __displayContext.pitch;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get frame buffer pointers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned char* wgfx_get_win_pfb(int winIdx, int col, int row)
{
    return __displayContext.pfb + 
            ((row * __wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale) +
            __wgfxWindows[winIdx].tly) * __displayContext.pitch + 
            (col * __wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale) + 
            __wgfxWindows[winIdx].tlx;
}

unsigned char* wgfx_get_win_pfb_xy(int winIdx, int x, int y)
{
    return __displayContext.pfb + 
            ((y * __wgfxWindows[winIdx].yPixScale) + __wgfxWindows[winIdx].tly) * __displayContext.pitch + 
            (x * __wgfxWindows[winIdx].xPixScale) + __wgfxWindows[winIdx].tlx;
}

unsigned char* wgfx_get_pfb_xy(int x, int y)
{
    return __displayContext.pfb + y * __displayContext.pitch + x;
}

void wgfx_restore_cursor_content()
{
    // Write content of cell buffer to current screen location
    wgfx_write_cell(__displayContext.term.outputWinIdx, __displayContext.term.cursor_col, __displayContext.term.cursor_row, __displayContext.cursor_buffer);
}

void wgfx_term_render_cursor()
{
    // Read content of cell buffer to current screen location
    wgfx_read_cell(__displayContext.term.outputWinIdx, __displayContext.term.cursor_col, __displayContext.term.cursor_row, __displayContext.cursor_buffer);
    // Show cursor
    wgfx_putc(__displayContext.term.outputWinIdx, __displayContext.term.cursor_col, __displayContext.term.cursor_row, '_');
}

void wgfx_set_bg(DISPLAY_FX_COL col)
{
    __displayContext.bg = col;
}

void wgfx_set_fg(DISPLAY_FX_COL col)
{
    __displayContext.fg = col;
}

void wgfx_wait_for_prev_operation()
{
    if (__wgfxDMAActive)
        for (int i = 0; i < 100000; i++)
            if (!dma_running())
                break;
    __wgfxDMAActive = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scroll
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define USE_DMA_FOR_SCROLL 1

// Positive values for rows scroll up, negative down
void wgfx_scroll(int winIdx, int rows)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS || rows == 0)
        return;

    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();
    
    // Get framebuffer location
    int numRows = rows < 0 ? -rows : rows;
    unsigned char* pBlankStart = NULL;
    unsigned char* pBlankEnd = NULL;
    if (rows > 0)
    {
        unsigned char* pDest = wgfx_get_win_pfb(winIdx, 0, 0);
        unsigned char* pSrc = wgfx_get_win_pfb(winIdx, 0, numRows);
        unsigned char* pEnd = wgfx_get_win_pfb(winIdx, 0, __displayContext.term.numRows);
        pBlankStart = wgfx_get_win_pfb(winIdx, 0, __displayContext.term.numRows-numRows);
        pBlankEnd = pEnd;

#ifdef USE_DMA_FOR_SCROLL
    unsigned int* BG = (unsigned int*)lowlev_mem_2uncached( mem_buff_dma );
    *BG = __displayContext.bg<<24 | __displayContext.bg<<16 | __displayContext.bg<<8 | __displayContext.bg;
    *(BG+1) = *BG;
    *(BG+2) = *BG;
    *(BG+3) = *BG;
    // unsigned int line_height = __displayContext.Pitch * npixels;

    dma_enqueue_operation( (unsigned int *)( pSrc ),
                           (unsigned int *)( pDest ),
                           (pEnd-pSrc),
                           0,
                           DMA_TI_SRC_INC | DMA_TI_DEST_INC );

    dma_enqueue_operation( BG,
                           (unsigned int *)( pBlankStart ),
                           pBlankEnd-pBlankStart,
                           0,
                           DMA_TI_DEST_INC );

    dma_execute_queue();
    __wgfxDMAActive = true;

#else        
        while (pSrc < pEnd)
        {
            *pDest++ = *pSrc++;
        }
#endif
    }
    else
    {
        unsigned char* pDest = wgfx_get_win_pfb(winIdx, 0, __displayContext.term.numRows) - 1;
        unsigned char* pSrc = wgfx_get_win_pfb(winIdx, 0, __displayContext.term.numRows-numRows) - 1;
        unsigned char* pEnd = wgfx_get_win_pfb(winIdx, 0, 0);
        pBlankStart = pEnd;
        pBlankEnd = wgfx_get_win_pfb(winIdx, 0, numRows);
        pBlankEnd = pSrc;
        while (pSrc > pEnd)
        {
            *pDest-- = *pSrc--;            
        }
    }

    // // Clear lines
    // while(pBlankStart < pBlankEnd)
    // {
    //     *pBlankStart++ = __displayContext.bg;
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Line functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wgfxHLine(int x, int y, int len, int colour)
{
    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();

    unsigned char* pBuf = wgfx_get_pfb_xy(x, y);
    for (int i = 0; i < len; i++)
    {
        *pBuf++ = colour;
    }
}

void wgfxVLine(int x, int y, int len, int colour)
{
    // Wait for previous operation (dma) to complete
    wgfx_wait_for_prev_operation();
    unsigned char* pBuf = wgfx_get_pfb_xy(x, y);
    for (int i = 0; i < len; i++)
    {
        *pBuf = colour;
        pBuf += __displayContext.pitch;
    }
}
