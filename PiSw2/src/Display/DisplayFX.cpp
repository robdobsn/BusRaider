// Bus Raider
// Rob Dobson 2019

#include "DisplayFX.h"
#include "logging.h"
#include <string.h>
#include "lowlib.h"
#include "lowlev.h"

static const unsigned int xterm_colors[256] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 
    0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff, 
    0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000df, 0x0000ff, 0x005f00, 0x005f5f,
    0x005f87, 0x005faf, 0x005fdf, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af, 
    0x0087df, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afdf, 0x00afff,
    0x00df00, 0x00df5f, 0x00df87, 0x00dfaf, 0x00dfdf, 0x00dfff, 0x00ff00, 0x00ff5f, 
    0x00ff87, 0x00ffaf, 0x00ffdf, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af, 
    0x5f00df, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fdf, 0x5f5fff, 
    0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87df, 0x5f87ff, 0x5faf00, 0x5faf5f,
    0x5faf87, 0x5fafaf, 0x5fafdf, 0x5fafff, 0x5fdf00, 0x5fdf5f, 0x5fdf87, 0x5fdfaf,
    0x5fdfdf, 0x5fdfff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffdf, 0x5fffff, 
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700df, 0x8700ff, 0x875f00, 0x875f5f, 
    0x875f87, 0x875faf, 0x875fdf, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af, 
    0x8787df, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afdf, 0x87afff, 
    0x87df00, 0x87df5f, 0x87df87, 0x87dfaf, 0x87dfdf, 0x87dfff, 0x87ff00, 0x87ff5f,
    0x87ff87, 0x87ffaf, 0x87ffdf, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af, 
    0xaf00df, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fdf, 0xaf5fff,
    0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87df, 0xaf87ff, 0xafaf00, 0xafaf5f,
    0xafaf87, 0xafafaf, 0xafafdf, 0xafafff, 0xafdf00, 0xafdf5f, 0xafdf87, 0xafdfaf, 
    0xafdfdf, 0xafdfff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffdf, 0xafffff,
    0xdf0000, 0xdf005f, 0xdf0087, 0xdf00af, 0xdf00df, 0xdf00ff, 0xdf5f00, 0xdf5f5f,
    0xdf5f87, 0xdf5faf, 0xdf5fdf, 0xdf5fff, 0xdf8700, 0xdf875f, 0xdf8787, 0xdf87af, 
    0xdf87df, 0xdf87ff, 0xdfaf00, 0xdfaf5f, 0xdfaf87, 0xdfafaf, 0xdfafdf, 0xdfafff,
    0xdfdf00, 0xdfdf5f, 0xdfdf87, 0xdfdfaf, 0xdfdfdf, 0xdfdfff, 0xdfff00, 0xdfff5f, 
    0xdfff87, 0xdfffaf, 0xdfffdf, 0xdfffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
    0xff00df, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fdf, 0xff5fff, 
    0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87df, 0xff87ff, 0xffaf00, 0xffaf5f,
    0xffaf87, 0xffafaf, 0xffafdf, 0xffafff, 0xffdf00, 0xffdf5f, 0xffdf87, 0xffdfaf, 
    0xffdfdf, 0xffdfff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffdf, 0xffffff, 
    0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,
    0x585858, 0x606060, 0x666666, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
    0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee
};

DisplayFX::DisplayFX() :
    _pBCMFrameBuffer(NULL)
{
    _screenWidth = 0;
    _screenHeight = 0;
    _pitch = 0;
    _size = 0;
    _pRawFrameBuffer = NULL;
    _consoleWinIdx = 0;
    _screenBackground = DISPLAY_FX_BLACK;
    _screenForeground = DISPLAY_FX_WHITE;
}

DisplayFX::~DisplayFX()
{
	delete _pBCMFrameBuffer;
}

bool DisplayFX::init(int displayWidth, int displayHeight)
{
    _pBCMFrameBuffer = new CBcmFrameBuffer (displayWidth, displayHeight, DEPTH);
#if DEPTH == 8
    for (int i = 0; i < 256; i++)
        _pBCMFrameBuffer->SetPalette32(i, COLORTRANS(xterm_colors[i]));
    // _pBCMFrameBuffer->SetPalette (NORMAL_COLOR, NORMAL_COLOR16);
    // _pBCMFrameBuffer->SetPalette (HIGH_COLOR,   HIGH_COLOR16);
    // _pBCMFrameBuffer->SetPalette (HALF_COLOR,   HALF_COLOR16);
#endif
    if ((!_pBCMFrameBuffer) || (!_pBCMFrameBuffer->Initialize ()))
    {
        return FALSE;
    }

    if (_pBCMFrameBuffer->GetDepth () != DEPTH)
    {
        return FALSE;
    }

    _pRawFrameBuffer = (uint8_t*)_pBCMFrameBuffer->GetBuffer ();
    _size   = _pBCMFrameBuffer->GetSize ();
    _pitch  = _pBCMFrameBuffer->GetPitch ();
    _screenWidth  = _pBCMFrameBuffer->GetWidth ();
    _screenHeight = _pBCMFrameBuffer->GetHeight ();

    // Ensure that each row is word-aligned so that we can safely use memcpyblk()
    if (_pitch % sizeof (u32) != 0)
    {
        return FALSE;
    }
    _pitch /= sizeof (uint8_t);

    screenClear();

    // Initial full-screen window
    windowSetup(0, 0, 0, displayWidth, displayHeight, -1, -1, 2, 2, NULL, -1, -1, 0, 0);

    // Reset window validity
    for (int i = 0; i < DISPLAY_FX_MAX_WINDOWS; i++)
        _windows[i]._valid = false;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Screen handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::screenClear()
{
    uint8_t* pFrameBuf = _pRawFrameBuffer;
    uint8_t* pFBEnd = pFrameBuf + _size;
    while (pFrameBuf < pFBEnd)
        *pFrameBuf++ = _screenBackground;
}

void DisplayFX::screenRectClear(int tlx, int tly, int width, int height)
{
    uint8_t* pDest = screenGetPFBXY(tlx, tly);
    int bytesAcross = width;
    int pixDown = height;
    for (int i = 0; i < pixDown; i++)
    {
        memset(pDest, _screenBackground, bytesAcross);
        pDest += _pitch;
    }
}

void DisplayFX::screenBackground(DISPLAY_FX_COLOUR colour)
{
    _screenBackground = colour;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::windowPut(int winIdx, int col, int row, const char* pStr)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    while(*pStr)
    {
        if (col >= _windows[winIdx].cols())
            break;
        windowPut(winIdx, col++, row, *pStr++);
    }
}

void DisplayFX::windowPut(int winIdx, int col, int row, int ch)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (!_windows[winIdx]._valid)
        return;
    if (col >= _windows[winIdx].cols())
        return;
    if (row >= _windows[winIdx].rows())
        return;

    // Pointer to framebuffer where char cell starts

    uint8_t* pBuf = windowGetPFB(winIdx, col, row);
    // Pointer to font data to write into char cell
    uint8_t* pFont = _windows[winIdx].pFont->pFontData + ch * _windows[winIdx].pFont->bytesPerChar;

    // For each bit in the font character write the appropriate data to the pixel in framebuffer
    uint8_t* pBufCur = pBuf;
    int fgColour = (_windows[winIdx].windowForeground != -1) ? _windows[winIdx].windowForeground : _screenForeground;
    int bgColour = (_windows[winIdx].windowBackground != -1) ? _windows[winIdx].windowBackground : _screenBackground;
    int cellHeight = _windows[winIdx].cellHeight;
    int yPixScale = _windows[winIdx].yPixScale;
    int cellWidth = _windows[winIdx].cellWidth;
    int xPixScale = _windows[winIdx].xPixScale;
    for (int y = 0; y < cellHeight; y++) {
        for (int i = 0; i < yPixScale; i++) {
            uint8_t* pFontCur = pFont;
            pBufCur = pBuf;
            int bitMask = 0x80;
            for (int x = 0; x < cellWidth; x++) {
                for (register int j = 0; j < xPixScale; j++) {
                    *pBufCur = (*pFontCur & bitMask) ? fgColour : bgColour;
                    pBufCur++;
                }
                bitMask = bitMask >> 1;
                if (bitMask == 0)
                {
                    bitMask = 0x80;
                    pFontCur++;
                }
            }
            pBuf += _pitch;
        }
        pFont += _windows[winIdx].pFont->bytesAcross;
    }
}

void DisplayFX::windowForeground(int winIdx, DISPLAY_FX_COLOUR colour)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    _windows[winIdx].windowForeground = colour;
}

void DisplayFX::windowBackground(int winIdx, DISPLAY_FX_COLOUR colour)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    _windows[winIdx].windowBackground = colour;
}

void DisplayFX::windowSetPixel(int winIdx, int x, int y, int value, DISPLAY_FX_COLOUR colour)
{
    unsigned char* pBuf = windowGetPFBXY(winIdx, x, y);
    int fgColour = ((_windows[winIdx].windowForeground != -1) ?
                    _windows[winIdx].windowForeground : _screenForeground);
    if (colour != -1)
        fgColour = colour;
    int bgColour = ((_windows[winIdx].windowBackground != -1) ?
                    _windows[winIdx].windowBackground : _screenBackground);
    uint8_t pixColour = value ? fgColour : bgColour;
    if ((_windows[winIdx].xPixScale & 0x03) == 0)
    {
        uint32_t pixColourL = (pixColour << 24) + (pixColour << 16) + (pixColour << 8) + pixColour;
        for (int iy = 0; iy < _windows[winIdx].yPixScale; iy++)
        {
            uint32_t* pBufL = (uint32_t*) (pBuf + iy * _pitch);
            for (int ix = 0; ix < _windows[winIdx].xPixScale/4; ix++)
                *pBufL++ = pixColourL;
        }
    }
    else
    {
        for (int iy = 0; iy < _windows[winIdx].yPixScale; iy++)
        {
            unsigned char* pBufL = pBuf + iy * _pitch;
            for (int ix = 0; ix < _windows[winIdx].xPixScale; ix++)
                *pBufL++ = pixColour;
        }
    }
}

void DisplayFX::getFramebuffer(int winIdx, FrameBufferInfo& frameBufferInfo)
{
    frameBufferInfo.pFB = _pRawFrameBuffer;
    frameBufferInfo.pixelsWidth = _screenWidth;
    frameBufferInfo.pixelsHeight = _screenWidth;
    frameBufferInfo.pitch = _pitch;
    frameBufferInfo.pFBWindow = windowGetPFBXY(winIdx, 0, 0);
    frameBufferInfo.pixelsWidthWindow = _windows[winIdx].width;
    frameBufferInfo.pixelsHeightWindow = _windows[winIdx].height;
    frameBufferInfo.bytesPerPixel = 1;
}

void DisplayFX::windowSetup(int winIdx, int tlx, int tly, int width, int height,
    int cellWidth, int cellHeight, int xPixScale, int yPixScale,
    WgfxFont* pFont, int foregroundColour, int backgroundColour, 
    int borderWidth, int borderColour)
{
    // Check window valid
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;

    // Default font if required
    WgfxFont* pFontToUse = (pFont != NULL) ? pFont : (&__systemFont);

    // Width and height
    if (width == -1 && tlx != -1)
        _windows[winIdx].width = _screenWidth - tlx - borderWidth * 2;
    else if (width == -1)
        _windows[winIdx].width = _screenWidth;
    else
        _windows[winIdx].width = width * xPixScale;
    if (height == -1)
        _windows[winIdx].height = _screenHeight - tly - borderWidth * 2;
    else
        _windows[winIdx].height = height * yPixScale;
    // Top,Left
    if (tlx != -1)
        _windows[winIdx].tlx = tlx * xPixScale + borderWidth;
    else
        _windows[winIdx].tlx = (_screenWidth - _windows[winIdx].width) / 2;
    _windows[winIdx].tly = tly * yPixScale + borderWidth;

    // Cell
    if (cellWidth == -1)
        _windows[winIdx].cellWidth = pFontToUse->cellX;
    else
        _windows[winIdx].cellWidth = cellWidth;
    if (cellHeight == -1)
        _windows[winIdx].cellHeight = pFontToUse->cellY;
    else
        _windows[winIdx].cellHeight = cellHeight;

    // Scale
    _windows[winIdx].xPixScale = xPixScale;
    _windows[winIdx].yPixScale = yPixScale;
    
    // Font
    _windows[winIdx].pFont = pFontToUse;
    _windows[winIdx].windowForeground = foregroundColour;
    _windows[winIdx].windowBackground = backgroundColour;

    // Border
    if (borderColour != -1 && borderWidth > 0)
    {
        for (int i = 0; i < borderWidth; i++)
        {
            drawHorizontal(_windows[winIdx].tlx-borderWidth, 
                    _windows[winIdx].tly-borderWidth+i, 
                    _windows[winIdx].width + borderWidth * 2,
                    borderColour);
            drawHorizontal(_windows[winIdx].tlx-borderWidth, 
                    _windows[winIdx].tly+_windows[winIdx].height+i, 
                    _windows[winIdx].width + borderWidth * 2,
                    borderColour);
            drawVertical(_windows[winIdx].tlx-borderWidth+i, 
                    _windows[winIdx].tly-borderWidth, 
                    _windows[winIdx].height + borderWidth * 2,
                    borderColour);
            drawVertical(_windows[winIdx].tlx+_windows[winIdx].width+i, 
                    _windows[winIdx].tly-borderWidth, 
                    _windows[winIdx].height + borderWidth * 2,
                    borderColour);
        }
    }

    // uart_printf("idx %d, cx %d cy %d sx %d sy %d *pFont %02x %02x %02x\n\r\n",
    //     winIdx,
    //     _windows[winIdx].pFont->cellX,
    //     _windows[winIdx].pFont->cellY,
    //     _windows[winIdx].xPixScale,
    //     _windows[winIdx].yPixScale,
    //     pFontToUse->pFontData[0],
    //     pFontToUse->pFontData[1],
    //     pFontToUse->pFontData[2]);

    _windows[winIdx]._valid = true;
}

void DisplayFX::windowClear(int winIdx)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (!_windows[winIdx]._valid)
        return;

    uint8_t* pDest = windowGetPFB(winIdx, 0, 0);
    int bytesAcross = _windows[winIdx].width;
    int pixDown = _windows[winIdx].height;
    for (int i = 0; i < pixDown; i++)
    {
        memset(pDest, _screenBackground, bytesAcross);
        pDest += _pitch;
    }
}

uint8_t* DisplayFX::windowGetPFB(int winIdx, int col, int row)
{
    return _pRawFrameBuffer + ((row * _windows[winIdx].cellHeight * _windows[winIdx].yPixScale) + _windows[winIdx].tly) * _pitch + 
            (col * _windows[winIdx].cellWidth * _windows[winIdx].xPixScale) + _windows[winIdx].tlx;
}

uint8_t* DisplayFX::screenGetPFBXY(int x, int y)
{
    return _pRawFrameBuffer + y * _pitch + x;
}

uint8_t* DisplayFX::windowGetPFBXY(int winIdx, int x, int y)
{
    return _pRawFrameBuffer + 
            ((y * _windows[winIdx].yPixScale) + _windows[winIdx].tly) * _pitch + 
            (x * _windows[winIdx].xPixScale) + _windows[winIdx].tlx;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put to console window
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MIN(v1, v2) (((v1) < (v2)) ? (v1) : (v2))
#define MAX(v1, v2) (((v1) > (v2)) ? (v1) : (v2))

void DisplayFX::consolePut(int ch)
{

    switch (ch) {
    case '\r':
        _windows[_consoleWinIdx]._cursorCol = 0;
        break;

    case '\n':
        _windows[_consoleWinIdx]._cursorCol = 0;
        _windows[_consoleWinIdx]._cursorRow++;
        cursorCheck();
        break;

    case 0x09: /* tab */
        _windows[_consoleWinIdx]._cursorCol += 1;
        _windows[_consoleWinIdx]._cursorCol = MIN(_windows[_consoleWinIdx]._cursorCol + 8 - 
                    _windows[_consoleWinIdx]._cursorCol % 8, _windows[_consoleWinIdx].cols() - 1);
        cursorCheck();
        break;

    case 0x08:
        /* backspace */
        if (_windows[_consoleWinIdx]._cursorCol > 0) {
            _windows[_consoleWinIdx]._cursorCol--;
            windowPut(_consoleWinIdx, _windows[_consoleWinIdx]._cursorCol, 
                            _windows[_consoleWinIdx]._cursorRow, ' ');
        }
        break;

    default:
        windowPut(_consoleWinIdx, _windows[_consoleWinIdx]._cursorCol, 
                            _windows[_consoleWinIdx]._cursorRow, ch);
        _windows[_consoleWinIdx]._cursorCol++;
        cursorCheck();
        break;
    }
}

void DisplayFX::consolePut(const char* pStr)
{
    //TODO
    // Restore cursor
    // cursorRestore();

    // Write string
    while (*pStr)
        consolePut(*pStr++);

//TODO
    // Render cursor
    // cursorRender();
}

void DisplayFX::consolePut(const char* pBuffer, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
        consolePut(pBuffer[i]);
}

void DisplayFX::consoleForeground(DISPLAY_FX_COLOUR colour)
{
    _windows[_consoleWinIdx].windowForeground = colour;
}

int DisplayFX::consoleGetWidth()
{
    return _windows[_consoleWinIdx].cols();
}

void DisplayFX::consoleSetWindow(int winIdx)
{
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    _consoleWinIdx = winIdx;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cursor handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::cursorRestore()
{
    // Validity
    if (!_windows[_consoleWinIdx]._valid)
        return;
    
    // Write content of cell buffer to current screen location
    screenReadCell(_consoleWinIdx, _windows[_consoleWinIdx]._cursorCol,
            _windows[_consoleWinIdx]._cursorRow, _windows[_consoleWinIdx]._cursorBuffer);
}

void DisplayFX::cursorRender()
{
    // Validity
    if (!_windows[_consoleWinIdx]._valid)
        return;

    // Read content of cell buffer to current screen location
    screenWriteCell(_consoleWinIdx, _windows[_consoleWinIdx]._cursorCol,
            _windows[_consoleWinIdx]._cursorRow, _windows[_consoleWinIdx]._cursorBuffer);

    // Show cursor
    windowPut(_consoleWinIdx, _windows[_consoleWinIdx]._cursorCol,
            _windows[_consoleWinIdx]._cursorRow, '_');
}

void DisplayFX::cursorCheck()
{
    if (_windows[_consoleWinIdx]._cursorCol >= _windows[_consoleWinIdx].cols())
    {
        _windows[_consoleWinIdx]._cursorRow++;
        _windows[_consoleWinIdx]._cursorCol = 0;
    }

    if (_windows[_consoleWinIdx]._cursorRow >= _windows[_consoleWinIdx].rows()) 
    {
        _windows[_consoleWinIdx]._cursorRow--;
        windowScroll(_consoleWinIdx, 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scroll
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::windowScroll(int winIdx, int rows)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS || rows == 0)
        return;

    // Get framebuffer location
    int numRows = rows < 0 ? -rows : rows;
    if (rows > 0)
    {
        uint8_t* pDest = windowGetPFB(winIdx, 0, 0);
        uint8_t* pSrc = windowGetPFB(winIdx, 0, numRows);
        int bytesAcross = _windows[winIdx].cols() * _windows[winIdx].cellWidth * _windows[winIdx].xPixScale;
        int pixDown = _windows[winIdx].rows() * _windows[winIdx].cellHeight * _windows[winIdx].yPixScale;
        for (int i = 0; i < pixDown; i++)
        {
            memcopyfast(pDest, pSrc, bytesAcross);
            pDest += _pitch;
            pSrc += _pitch;
        }
    }
    else
    {
        uint8_t* pDest = windowGetPFB(winIdx, 0, _windows[winIdx].rows()) - 1;
        uint8_t* pSrc = windowGetPFB(winIdx, 0, _windows[winIdx].rows()-numRows) - 1;
        uint8_t* pEnd = windowGetPFB(winIdx, 0, 0);
        while (pSrc > pEnd)
        {
            *pDest-- = *pSrc--;            
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Drawing functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::drawHorizontal(int x, int y, int len, int colour)
{
    uint8_t* pBuf = screenGetPFBXY(x, y);
    for (int i = 0; i < len; i++)
    {
        *pBuf++ = colour;
    }
}

void DisplayFX::drawVertical(int x, int y, int len, int colour)
{
    uint8_t* pBuf = screenGetPFBXY(x, y);
    for (int i = 0; i < len; i++)
    {
        *pBuf = colour;
        pBuf += _pitch;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Drawing functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFX::screenReadCell(int winIdx, int col, int row, uint8_t* pCellBuf)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (col >= _windows[winIdx].cols())
        return;
    if (row >= _windows[winIdx].rows())
        return;
    if (!_windows[winIdx]._valid)
        return;

    // Pointer to framebuffer where char cell starts
    uint8_t* pBuf = windowGetPFB(winIdx, col, row);

    // Write data from cell buffer
    uint8_t* pBufCur = pBuf;
    for (int y = 0; y < _windows[winIdx].cellHeight; y++) {
        for (int i = 0; i < _windows[winIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < _windows[winIdx].cellWidth; x++) {
                for (int j = 0; j < _windows[winIdx].xPixScale; j++) {
                    *pCellBuf++ = *pBufCur++;
                }
            }
            pBuf += _pitch;
        }
    }
}

void DisplayFX::screenWriteCell(int winIdx, int col, int row, uint8_t* pCellBuf)
{
    // Validity
    if (winIdx < 0 || winIdx >= DISPLAY_FX_MAX_WINDOWS)
        return;
    if (col >= _windows[winIdx].cols())
        return;
    if (row >= _windows[winIdx].rows())
        return;
    if (!_windows[winIdx]._valid)
        return;

    // Pointer to framebuffer where char cell starts
    uint8_t* pBuf = windowGetPFB(winIdx, col, row);

    // Write data from cell buffer
    uint8_t* pBufCur = pBuf;
    for (int y = 0; y < _windows[winIdx].cellHeight; y++) {
        for (int i = 0; i < _windows[winIdx].yPixScale; i++) {
            pBufCur = pBuf;
            for (int x = 0; x < _windows[winIdx].cellWidth; x++) {
                for (int j = 0; j < _windows[winIdx].xPixScale; j++) {
                    *pBufCur++ = *pCellBuf++;
                }
            }
            pBuf += _pitch;
        }
    }
}