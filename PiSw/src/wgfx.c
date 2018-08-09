// Pi Bus Raider
// Rob Dobson 2018

#include "wgfx.h"
#include "ee_printf.h"

extern WgfxFont __systemFont;

#define MIN(v1, v2) (((v1) < (v2)) ? (v1) : (v2))
#define MAX(v1, v2) (((v1) > (v2)) ? (v1) : (v2))

typedef struct WgfxWindowDef {
    int tlx;
    int width;
    int tly;
    int height;
    int cellWidth;
    int cellHeight;
    int xPixScale;
    int yPixScale;
    WgfxFont* pFont;
} WgfxWindowDef;

#define WGFX_MAX_WINDOWS 5
static WgfxWindowDef __wgfxWindows[WGFX_MAX_WINDOWS];
static int __wgfxNumWindows = 0;

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

    WGFX_COL bg;
    WGFX_COL fg;

    // Make sure this is big enough for any font's character cell
    unsigned char cursor_buffer[512];

} FRAMEBUFFER_CTX;

static FRAMEBUFFER_CTX ctx;

void wgfx_set_window(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont)
{
    WgfxFont* pFontToUse = (pFont != NULL) ? pFont : (&__systemFont);
    if (winIdx < 0 || winIdx >= WGFX_MAX_WINDOWS)
        return;
    __wgfxWindows[winIdx].tlx = tlx * xPixScale;
    __wgfxWindows[winIdx].tly = tly * yPixScale;
    if (width == -1)
        __wgfxWindows[winIdx].width = ctx.screenWidth - tlx;
    else
        __wgfxWindows[winIdx].width = width * xPixScale;
    if (height == -1)
        __wgfxWindows[winIdx].height = ctx.screenHeight - tly;
    else
        __wgfxWindows[winIdx].height = height * yPixScale;
    if (cellWidth == -1)
        __wgfxWindows[winIdx].cellWidth = pFontToUse->cellX;
    else
        __wgfxWindows[winIdx].cellWidth = cellWidth;
    if (cellHeight == -1)
        __wgfxWindows[winIdx].cellHeight = pFontToUse->cellY;
    else
        __wgfxWindows[winIdx].cellHeight = cellHeight;
    __wgfxWindows[winIdx].xPixScale = xPixScale;
    __wgfxWindows[winIdx].yPixScale = yPixScale;
    __wgfxWindows[winIdx].pFont = pFontToUse;

    // uart_printf("idx %d, cx %d cy %d sx %d sy %d *pFont %02x %02x %02x\n\r\n",
    //     winIdx,
    //     __wgfxWindows[winIdx].pFont->cellX,
    //     __wgfxWindows[winIdx].pFont->cellY,
    //     __wgfxWindows[winIdx].xPixScale,
    //     __wgfxWindows[winIdx].yPixScale,
    //     pFontToUse->pFontData[0],
    //     pFontToUse->pFontData[1],
    //     pFontToUse->pFontData[2]);

    __wgfxNumWindows = winIdx + 1;
}

void wgfx_set_console_window(int winIdx)
{
    if (winIdx < 0 || winIdx >= WGFX_MAX_WINDOWS)
        return;
    ctx.term.outputWinIdx = winIdx;
    // Reset term params
    ctx.term.numCols = __wgfxWindows[winIdx].width / (__wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale);
    ctx.term.numRows = __wgfxWindows[winIdx].height / (__wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale);
    ctx.term.cursor_row = ctx.term.cursor_col = 0;
}

void wgfx_term_render_cursor();
void wgfx_restore_cursor_content();
unsigned char* wgfx_get_win_pfb(int winIdx, int col, int row);

void wgfx_init(void* p_framebuffer, unsigned int width, unsigned int height,
    unsigned int pitch, unsigned int size)
{
    ctx.pfb = p_framebuffer;
    ctx.screenWidth = width;
    ctx.screenHeight = height;
    ctx.pitch = pitch;
    ctx.size = size;

    // Windows
    wgfx_set_window(0, 0, 0, width, height, -1, -1, 2, 2, NULL);

    // Initial settings
    ctx.term.numCols = ctx.screenWidth / __systemFont.cellX;
    ctx.term.numRows = ctx.screenHeight / __systemFont.cellY;
    ctx.term.cursor_row = ctx.term.cursor_col = 0;
    ctx.term.cursor_visible = 1;
    ctx.term.outputWinIdx = 0;

    ctx.bg = 0;
    ctx.fg = 15;
    wgfx_term_render_cursor();
}

void wgfx_clear()
{
    unsigned char* pf = ctx.pfb;
    unsigned char* pfb_end = pf + ctx.size;
    while (pf < pfb_end)
        *pf++ = ctx.bg;
}

void wgfx_clear_screen()
{
    wgfx_clear();
    wgfx_term_render_cursor();
}

void wgfx_check_curpos()
{
    if (ctx.term.cursor_col >= ctx.term.numCols) {
        ctx.term.cursor_row++;
        ctx.term.cursor_col = 0;
    }

    if (ctx.term.cursor_row >= ctx.term.numRows) {
        --ctx.term.cursor_row;
        wgfx_scroll(ctx.term.outputWinIdx, 1);
    }
}

void wgfx_term_putchar(char ch)
{
    switch (ch) {
    case '\r':
        ctx.term.cursor_col = 0;
        break;

    case '\n':
        ctx.term.cursor_col = 0;
        ctx.term.cursor_row++;
        wgfx_check_curpos();
        break;

    case 0x09: /* tab */
        ctx.term.cursor_col += 1;
        ctx.term.cursor_col = MIN(ctx.term.cursor_col + 8 - ctx.term.cursor_col % 8, ctx.term.numCols - 1);
        wgfx_check_curpos();
        break;

    case 0x08:
        /* backspace */
        if (ctx.term.cursor_col > 0) {
            ctx.term.cursor_col--;
            wgfx_putc(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row, ' ');
        }
        break;

    default:
        wgfx_putc(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row, ch);
        ctx.term.cursor_col++;
        wgfx_check_curpos();
        break;
    }
}

void wgfx_term_putstring(const char* str)
{
    wgfx_restore_cursor_content();
    while (*str)
        wgfx_term_putchar(*str++);
    wgfx_term_render_cursor();
}

void wgfx_putc(int windowIdx, unsigned int col, unsigned int row, unsigned char ch)
{
    if (col >= ctx.term.numCols)
        return;
    if (row >= ctx.term.numRows)
        return;
    if (windowIdx < 0 || windowIdx >= __wgfxNumWindows)
        return;

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(windowIdx, col, row);
    // Pointer to font data to write into char cell
    unsigned char* pFont = __wgfxWindows[windowIdx].pFont->pFontData + ch * __wgfxWindows[windowIdx].pFont->bytesPerChar;

    // For each bit in the font character write the appropriate data to the pixel in framebuffer
    unsigned char* pBufCur = pBuf;
    for (int y = 0; y < __wgfxWindows[windowIdx].cellHeight; y++) {
        for (int i = 0; i < __wgfxWindows[windowIdx].yPixScale; i++) {
            pBufCur = pBuf;
            int bitMask = 0x01 << (__wgfxWindows[windowIdx].cellWidth - 1);
            for (int x = 0; x < __wgfxWindows[windowIdx].cellWidth; x++) {
                for (int j = 0; j < __wgfxWindows[windowIdx].xPixScale; j++) {
                    *pBufCur = (*pFont & bitMask) ? ctx.fg : ctx.bg;
                    pBufCur++;
                }
                bitMask = bitMask >> 1;
            }
            pBuf += ctx.pitch;
        }
        pFont += __wgfxWindows[windowIdx].pFont->bytesAcross;
    }
}

// Write data from a char cell in a window
void wgfx_write_cell(int windowIdx, unsigned int col, unsigned int row, unsigned char* pCellBuf)
{
    if (col >= ctx.term.numCols)
        return;
    if (row >= ctx.term.numRows)
        return;
    if (windowIdx < 0 || windowIdx >= __wgfxNumWindows)
        return;

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(windowIdx, col, row);

    // Write data from cell buffer
    unsigned char* pBufCur = pBuf;
    for (int y = 0; y < __wgfxWindows[windowIdx].cellHeight; y++) {
        for (int i = 0; i < __wgfxWindows[windowIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < __wgfxWindows[windowIdx].cellWidth; x++) {
                for (int j = 0; j < __wgfxWindows[windowIdx].xPixScale; j++) {
                    *pBufCur++ = *pCellBuf++;
                }
            }
            pBuf += ctx.pitch;
        }
    }
}

// Read data from a char cell in a window
void wgfx_read_cell(int windowIdx, unsigned int col, unsigned int row, unsigned char* pCellBuf)
{
    if (col >= ctx.term.numCols)
        return;
    if (row >= ctx.term.numRows)
        return;
    if (windowIdx < 0 || windowIdx >= __wgfxNumWindows)
        return;

    // Pointer to framebuffer where char cell starts
    unsigned char* pBuf = wgfx_get_win_pfb(windowIdx, col, row);

    // Write data from cell buffer
    unsigned char* pBufCur = pBuf;
    for (int y = 0; y < __wgfxWindows[windowIdx].cellHeight; y++) {
        for (int i = 0; i < __wgfxWindows[windowIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < __wgfxWindows[windowIdx].cellWidth; x++) {
                for (int j = 0; j < __wgfxWindows[windowIdx].xPixScale; j++) {
                    *pCellBuf++ = *pBufCur++;
                }
            }
            pBuf += ctx.pitch;
        }
    }
}

unsigned char* wgfx_get_win_pfb(int winIdx, int col, int row)
{
    return ctx.pfb + ((row * __wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale) + __wgfxWindows[winIdx].tly) * ctx.pitch + (col * __wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale) + __wgfxWindows[winIdx].tlx;
}

void wgfx_restore_cursor_content()
{
    // Write content of cell buffer to current screen location
    wgfx_write_cell(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row, ctx.cursor_buffer);
}

void wgfx_term_render_cursor()
{
    // Read content of cell buffer to current screen location
    wgfx_read_cell(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row, ctx.cursor_buffer);
    // Show cursor
    wgfx_putc(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row, '_');
}

void wgfx_set_bg(WGFX_COL col)
{
    ctx.bg = col;
}

void wgfx_set_fg(WGFX_COL col)
{
    ctx.fg = col;
}

// Positive values for rows scroll up, negative down
void wgfx_scroll(int windowIdx, int rows)
{
    if (windowIdx < 0 || windowIdx >= WGFX_MAX_WINDOWS || rows == 0)
        return;
    int numRows = rows < 0 ? -rows : rows;
    unsigned char* pBlankStart = NULL;
    unsigned char* pBlankEnd = NULL;
    if (rows > 0)
    {
        unsigned char* pDest = wgfx_get_win_pfb(windowIdx, 0, 0);
        unsigned char* pSrc = wgfx_get_win_pfb(windowIdx, 0, numRows);
        unsigned char* pEnd = wgfx_get_win_pfb(windowIdx, 0, ctx.term.numRows);
        pBlankStart = wgfx_get_win_pfb(windowIdx, 0, ctx.term.numRows-numRows);
        pBlankEnd = pEnd;
        while (pSrc < pEnd)
        {
            *pDest++ = *pSrc++;
        }
    }
    else
    {
        unsigned char* pDest = wgfx_get_win_pfb(windowIdx, 0, ctx.term.numRows) - 1;
        unsigned char* pSrc = wgfx_get_win_pfb(windowIdx, 0, ctx.term.numRows-numRows) - 1;
        unsigned char* pEnd = wgfx_get_win_pfb(windowIdx, 0, 0);
        pBlankStart = pEnd;
        pBlankEnd = wgfx_get_win_pfb(windowIdx, 0, numRows);
        pBlankEnd = pSrc;
        while (pSrc > pEnd)
        {
            *pDest-- = *pSrc--;            
        }
    }

    // Clear lines
    while(pBlankStart < pBlankEnd)
    {
        *pBlankStart++ = ctx.bg;
    }

    // unsigned int* pf_dst = (unsigned int*)( ctx.pfb + ctx.size ) -1;
    // unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.size - ctx.Pitch*npixels) -1;
    // const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb );

    // while( pf_src >= pfb_end )
    //     *pf_dst-- = *pf_src--;

    // // Fill with bg at the top
    // const unsigned int BG = ctx.bg<<24 | ctx.bg<<16 | ctx.bg<<8 | ctx.bg;
    // while( pf_dst >= pfb_end )
    //     *pf_dst-- = BG;
}
