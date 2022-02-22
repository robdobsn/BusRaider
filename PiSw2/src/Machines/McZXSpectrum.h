// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McBase.h"
#include "usb_hid_keys.h"
#include "TargetRegisters.h"
#include "logging.h"

class McZXSpectrum : public McBase
{
public:

    McZXSpectrum(McManager& mcManager, BusControl& busControl);

    // Enable machine
    virtual void enableMachine() override;

    // Disable machine
    virtual void disableMachine() override;

    // Service
    virtual void service() override;

    // Machine heartbeat
    virtual void machineHeartbeat() override;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void refreshDisplay() override;

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) override;

    // Check if machine can process a file type
    virtual bool canProcFileType(const char* fileType) override;

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, 
                    int fileLen, TargetImager& targetImager) override;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) override;

    // Bus action active callback
    virtual void busReqAckedCallback(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt) override;

    // Clock frequency in Hz change
    static const uint32_t MIN_KEY_DOWN_TIME_IN_T_STATES = 750000;
    virtual void informClockFreqHz(uint32_t clockFreqHz) override
    {
        uint32_t clockInCyclesPerMs = clockFreqHz / 1000 + 1;
        _keyBuf.setMinKeyPeriodMs(MIN_KEY_DOWN_TIME_IN_T_STATES / clockInCyclesPerMs);
        // LogWrite(";;;", LOG_DEBUG, "informClockFreqHz %d clockPerMs %d minPeriodMs %d", 
        //             clockFreqHz, clockInCyclesPerMs, MIN_KEY_DOWN_TIME_IN_T_STATES / clockInCyclesPerMs);
    }

    // Get changes made since last mirror display update
    virtual uint32_t getMirrorChanges(uint8_t* pMirrorChangeBuf, uint32_t mirrorChangeMaxLen, bool forceGetAll) override;

private:
    static uint32_t getKeyBitmap(const int* keyCodes, int keyCodesLen, const uint8_t* currentKeyPresses);
    void updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen);

    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_ADDR = 0x4000;
    static constexpr uint32_t ZXSPECTRUM_DISP_RAM_SIZE = 0x1b00;
    uint8_t _screenBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _screenBufferValid;
    bool _screenRefreshRequired = false;
    uint32_t _screenBufferRefreshY;
    uint32_t _screenBufferRefreshX;
    uint32_t _screenBufferRefreshCount;
    uint8_t _screenCache[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _screenCacheValid;
    static const uint32_t MAX_BYTES_IN_EACH_MIRROR_UPDATE = 4096;
    uint32_t _nextMirrorPos = 0;
    uint8_t _mirrorCache[ZXSPECTRUM_DISP_RAM_SIZE];
    bool _mirrorCacheValid = false;

    uint8_t* _pFrameBuffer;
    uint32_t _pfbSize;
    uint32_t _framePitch;
    uint32_t _framePitchDiv4;
    uint32_t _scaleX;
    uint32_t _scaleY;
    uint32_t _lineStride;
    uint32_t _scaledStrideY;
    uint32_t _scaledStrideX;
    uint32_t _cellsX;
    uint32_t _cellsY;
    uint32_t _cellSizeY;
    uint32_t _cellSizeX;

    static constexpr uint32_t ZXSPECTRUM_PIXEL_RAM_SIZE = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_OFFSET = 0x1800;
    static constexpr uint32_t ZXSPECTRUM_COLOUR_DATA_SIZE = 0x300;

    static constexpr int ZXSPECTRUM_KEYBOARD_NUM_ROWS = 8;
    static constexpr int ZXSPECTRUM_KEYS_IN_ROW = 5;
    static constexpr int ZXSPECTRUM_BS_KEY_ROW_IDX = 0;
    static constexpr int ZXSPECTRUM_DEL_KEY_ROW_IDX = 4;
    static constexpr int ZXSPECTRUM_SHIFT_KEYS_ROW_IDX = 7;

    static McVariantTable _machineDescriptorTables[];

    static const int MAX_KEYS = 6;

    // static uint8_t _spectrumKeyboardIOBitMap[ZXSPECTRUM_KEYBOARD_NUM_ROWS];

    class KeyBits 
    {
    public:
        KeyBits()
        {
            clear();
        }
        KeyBits(const KeyBits& other)
        {
            // Clear key bitmap
            for (int i = 0; i < ZXSPECTRUM_KEYBOARD_NUM_ROWS; i++)
                bitMap[i] = other.bitMap[i];
        }
        void clear()
        {
            // Clear key bitmap
            for (int i = 0; i < ZXSPECTRUM_KEYBOARD_NUM_ROWS; i++)
                bitMap[i] = 0xff;
        }
        inline bool operator==(const KeyBits& other)
        {
            for (int i = 0; i < ZXSPECTRUM_KEYBOARD_NUM_ROWS; i++)
                if (bitMap[i] != other.bitMap[i])
                    return false;
            return true;
        }
        inline bool operator!=(const KeyBits& other)
        {
            return !(*this == other);
        }
        uint8_t bitMap[ZXSPECTRUM_KEYBOARD_NUM_ROWS];
    };
    class SpectrumKeyboardBuf 
    {
    public:
        static const uint32_t KEY_BUFFER_MAX_SIZE = 10;
        SpectrumKeyboardBuf()
        {
            _qHead = 0;
            _qCount = 0;
            _lastKeyChangeMs = 0;
        }
        bool isAvail()
        {
            return _qCount > 0;
        }
        bool isChanged()
        {
            return _scratchBits != _prevBits;
        }
        void clearScratch()
        {
            _scratchBits.clear();
        }
        void putFromScratch()
        {
            if (_qCount >= KEY_BUFFER_MAX_SIZE)
                return;
            if (!isChanged())
            {
                // LogWrite(";;;", LOG_DEBUG, "NOT CHAGNED");
                return;
            }
            _keyQueue[_qHead] = _scratchBits;
            _qHead = (_qHead + 1) % KEY_BUFFER_MAX_SIZE;
            _qCount++;
            _prevBits = _scratchBits;
            LogWrite(";;;", LOG_DEBUG, "PUT");
        }
        bool getToCurBitMap()
        {
            if (!isAvail())
                return false;
            uint32_t pos = (_qHead + KEY_BUFFER_MAX_SIZE - _qCount) % KEY_BUFFER_MAX_SIZE;
            _curBits = _keyQueue[pos];
            _qCount--;
            _lastKeyChangeMs = millis();
            LogWrite(";;;", LOG_DEBUG, "GOT");
            return true;
        }
        uint8_t getScratchBitMap(uint32_t idx)
        {
            if (idx >= ZXSPECTRUM_KEYBOARD_NUM_ROWS)
                return 0;
            return _scratchBits.bitMap[idx];
        }
        uint8_t getCurBitMap(uint32_t idx)
        {
            if (idx >= ZXSPECTRUM_KEYBOARD_NUM_ROWS)
                return 0;
            return _curBits.bitMap[idx];
        }
        void andScratch(uint32_t idx, uint32_t mask)
        {
            if (idx < ZXSPECTRUM_KEYBOARD_NUM_ROWS)
                _scratchBits.bitMap[idx] &= mask;
        }
        bool keyChangeReady()
        {
            if (!isAvail())
                return false;
            // LogWrite(";;;", LOG_DEBUG, "CheckReady avail %d millis %d last %d minPeriodMs %d diff %d", 
            //             isAvail(), millis(), _lastKeyChangeMs, _minPeriodKeyMs, millis()-_lastKeyChangeMs);
            if (isTimeout(millis(), _lastKeyChangeMs, _minPeriodKeyMs))
            {
                // LogWrite(";;;", LOG_DEBUG, "READY");
                return true;
            }
            return false;
        }
        void setMinKeyPeriodMs(uint32_t minPeriodMs)
        {
            _minPeriodKeyMs = minPeriodMs;
        }
        KeyBits _scratchBits;
        KeyBits _prevBits;
        KeyBits _curBits;
        KeyBits _keyQueue[KEY_BUFFER_MAX_SIZE];
        uint32_t _qHead;
        uint32_t _qCount;
        uint32_t _lastKeyChangeMs;
        uint32_t _minPeriodKeyMs;
    };
    SpectrumKeyboardBuf _keyBuf;
};
