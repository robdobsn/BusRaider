// Bus Raider
// Rob Dobson 2019

#include "DisplayFX.h"
#include "logging.h"
#include <string.h>
#include "lowlib.h"
#include "lowlev.h"

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
    _pBCMFrameBuffer->SetPalette (NORMAL_COLOR, NORMAL_COLOR16);
    _pBCMFrameBuffer->SetPalette (HIGH_COLOR,   HIGH_COLOR16);
    _pBCMFrameBuffer->SetPalette (HALF_COLOR,   HALF_COLOR16);
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
    // _pitch /= sizeof (uint8_t);

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