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

    unsigned int cursor_buffer[16];

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
    uart_printf("Here %d\r\n", winIdx);
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

    uart_printf("idx %d, cx %d cy %d sx %d sy %d *pFont %02x %02x %02x\n\r\n",
        winIdx,
        __wgfxWindows[winIdx].pFont->cellX,
        __wgfxWindows[winIdx].pFont->cellY,
        __wgfxWindows[winIdx].xPixScale,
        __wgfxWindows[winIdx].yPixScale,
        pFontToUse->pFontData[0],
        pFontToUse->pFontData[1],
        pFontToUse->pFontData[2]);

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

void wgfx_bump_curpos(int bumpRow)
{
    if (!bumpRow)
        ctx.term.cursor_col++;
    else
        ctx.term.cursor_row++;

    if (ctx.term.cursor_col >= ctx.term.numCols) {
        ctx.term.cursor_row++;
        ctx.term.cursor_col = 0;
    }

    if (ctx.term.cursor_row >= ctx.term.numRows) {
        --ctx.term.cursor_row;

        // gfx_scroll_down_dma(8);
        // gfx_term_render_cursor_newline_dma();
        // dma_execute_queue();
    }
}

void wgfx_term_putchar(char ch)
{
    switch (ch) {
    case '\r':
        wgfx_restore_cursor_content();
        ctx.term.cursor_col = 0;
        wgfx_term_render_cursor();
        break;

    case '\n':
        wgfx_restore_cursor_content();
        ctx.term.cursor_col = 0;
        wgfx_bump_curpos(1);
        wgfx_term_render_cursor();
        break;

    case 0x09: /* tab */
        wgfx_restore_cursor_content();
        ctx.term.cursor_col += 1;
        ctx.term.cursor_col = MIN(ctx.term.cursor_col + 8 - ctx.term.cursor_col % 8, ctx.term.numCols - 1);
        wgfx_term_render_cursor();
        break;

    case 0x08:
        /* backspace */
        if (ctx.term.cursor_col > 0) {
            wgfx_restore_cursor_content();
            ctx.term.cursor_col--;
            wgfx_putc(ctx.term.outputWinIdx, ctx.term.cursor_row, ctx.term.cursor_col, ' ');
            wgfx_term_render_cursor();
        }
        break;

    default:
        wgfx_restore_cursor_content();
        wgfx_putc(ctx.term.outputWinIdx, ctx.term.cursor_row, ctx.term.cursor_col, ch);
        wgfx_bump_curpos(0);
        wgfx_term_render_cursor();
        break;
    }
}

void wgfx_term_putstring(const char* str)
{
    while (*str)
        wgfx_term_putchar(*str++);
}

void wgfx_putc(int windowIdx, unsigned int row, unsigned int col, unsigned char ch)
{
    if (col >= ctx.term.numCols)
        return;
    if (row >= ctx.term.numRows)
        return;
    if (windowIdx < 0 || windowIdx >= __wgfxNumWindows)
        return;

    unsigned char* pBuf = wgfx_get_win_pfb(windowIdx, col, row); // ctx.pfb + row * 8 * ctx.Pitch + col * 8;
    unsigned char* pFont = __wgfxWindows[windowIdx].pFont->pFontData + ch * __wgfxWindows[windowIdx].pFont->bytesPerChar;

    // if (windowIdx == 1)
    // {
    // uart_printf("c %d r %d s %d idx %d, cx %d cy %d sx %d sy %d *pFont %02x %02x %02x\n\r",
    //                 col, row, ctx.pitch, windowIdx,
    //                 __wgfxWindows[windowIdx].cellWidth,
    //                 __wgfxWindows[windowIdx].cellHeight,
    //                 __wgfxWindows[windowIdx].xPixScale,
    //                 __wgfxWindows[windowIdx].yPixScale,
    //                 pFont[0], pFont[1], pFont[2]);
    // }

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

    // for (int l = 0; l < 100; l++)
    // {
    //     *pBuf = ctx.fg;
    //     pBuf += ctx.Pitch + 1;
    // }

    // for (int h = 0; h < 8; h++)
    // {
    //     for (int v = 0; v < 8; v++)
    //     {
    //         *pBuf++ = (ctx.fg & *pFontCh) | (ctx.bg & ~*pFontCh);
    //         pFontCh++;
    //     }
    //     pBuf += stride;
    // }

    // const unsigned int FG = ctx.fg<<24 | ctx.fg<<16 | ctx.fg<<8 | ctx.fg;
    // const unsigned int BG = ctx.bg<<24 | ctx.bg<<16 | ctx.bg<<8 | ctx.bg;
    // const unsigned int stride = (ctx.Pitch>>2) - 2;

    // register unsigned int* p_glyph = (unsigned int*)( FNT + ((unsigned int)ch<<6) );
    // register unsigned int* pf = (unsigned int*)PFB((col<<3), (row<<3));
    // register unsigned char h=8;

    // while(h--)
    // {
    //     //unsigned int w=2;
    //     //while( w-- ) // Loop unrolled for 8x8 fonts
    //     {
    //         register unsigned int gv = *p_glyph++;
    //         *pf++ =  (gv & FG) | ( ~gv & BG );
    //         gv = *p_glyph++;
    //         *pf++ =  (gv & FG) | ( ~gv & BG );
    //     }
    //     pf += stride;
    // }
}

unsigned char* wgfx_get_win_pfb(int winIdx, int col, int row)
{
    return ctx.pfb + ((row * __wgfxWindows[winIdx].cellHeight * __wgfxWindows[winIdx].yPixScale) + __wgfxWindows[winIdx].tly) * ctx.pitch + (col * __wgfxWindows[winIdx].cellWidth * __wgfxWindows[winIdx].xPixScale) + __wgfxWindows[winIdx].tlx;
}

void wgfx_restore_cursor_content()
{
    //     // Restore framebuffer content that was overwritten by the cursor
    //     unsigned int* pb = (unsigned int*)ctx.cursor_buffer;
    //     unsigned int* pfb = gfx_get_win_pfb(ctx.term.outputWinIdx, ctx.term.cursor_col, ctx.term.cursor_row);
    //     const unsigned int stride = (ctx.Pitch>>2) - 2;
    //     unsigned int h=8;
    //     while(h--)
    //     {
    //         *pfb++ = *pb++;
    //         *pfb++ = *pb++;
    //         pfb+=stride;
    //     }
    //     //cout("cursor restored");cout_d(ctx.term.cursor_row);cout("-");cout_d(ctx.term.cursor_col);cout_endl();
}

void wgfx_term_render_cursor()
{
    //     // Save framebuffer content that is going to be replaced by the cursor and update
    //     // the new content
    //     //
    //     unsigned int* pb = (unsigned int*)ctx.cursor_buffer;
    //     unsigned int* pfb = (unsigned int*)PFB( ctx.term.cursor_col*8, ctx.term.cursor_row*8 );
    //     const unsigned int stride = (ctx.Pitch>>2) - 2;
    //     unsigned int h=8;

    //     if( ctx.term.cursor_visible )
    //         while(h--)
    //         {
    //             *pb++ = *pfb; *pfb = ~*pfb; pfb++;
    //             *pb++ = *pfb; *pfb = ~*pfb; pfb++;
    //             pfb+=stride;
    //         }
    //     else
    //         while(h--)
    //         {
    //             *pb++ = *pfb++;
    //             *pb++ = *pfb++;
    //             pfb+=stride;
    //         }
}

void wgfx_set_bg(WGFX_COL col)
{
    ctx.bg = col;
}

void wgfx_set_fg(WGFX_COL col)
{
    ctx.fg = col;
}

// void wgfx_scroll(int windowIdx, int rows)
// {
//     int numRows = abs(rows);
//     unsigned char* pDest = rows < 0 ? wgfx_get_win_pfb(windowIdx, 0, 0) : wgfx_get_win_pfb(windowIdx, 0, numRows);
//     unsigned char* pSrc = rows < 0 ? wgfx_get_win_pfb(windowIdx, 0, numRows) : wgfx_get_win_pfb(windowIdx, 0, 0);

//     // unsigned int* pf_dst = (unsigned int*)( ctx.pfb + ctx.size ) -1;
//     // unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.size - ctx.Pitch*npixels) -1;
//     // const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb );

//     // while( pf_src >= pfb_end )
//     //     *pf_dst-- = *pf_src--;

//     // // Fill with bg at the top
//     // const unsigned int BG = ctx.bg<<24 | ctx.bg<<16 | ctx.bg<<8 | ctx.bg;
//     // while( pf_dst >= pfb_end )
//     //     *pf_dst-- = BG;
// }
