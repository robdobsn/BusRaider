
class DisplayChange
{
public:
    static const int MAX_DISP_CHANGE_OVERHEAD = 20;
    static const int FONT_ID_TRS80_L3 = 1;
    static const int FULL_SCREEN_UPDATE = 1;
    static const int CHAR_BASED_SCREEN = 0;
    static const int PIXEL_BASED_SCREEN = 1;
    static const int DISP_CHANGE_API_0 = 0;
    static const int SPECTRUM_SCREEN_LAYOUT = 1;

    static uint32_t initResponse(uint8_t* pBuf, uint32_t maxLen, 
                uint8_t updateType,
                uint16_t x, uint16_t y, 
                uint8_t screenAddressingType,
                uint8_t fontId,
                uint16_t bufferOffset,
                uint16_t totalDispBytes)
    {
        pBuf[0] = DISP_CHANGE_API_0;
        pBuf[1] = updateType;
        pBuf[2] = (x >> 8) & 0xff;
        pBuf[3] = x & 0xff;
        pBuf[4] = (y >> 8) & 0xff;
        pBuf[5] = y & 0xff;
        pBuf[6] = screenAddressingType;
        pBuf[7] = fontId;
        pBuf[8] = (bufferOffset >> 8) & 0xff;
        pBuf[9] = bufferOffset & 0xff;
        pBuf[10] = (totalDispBytes >> 8) & 0xff;
        pBuf[11] = totalDispBytes & 0xff;
        return 16;
    }
};