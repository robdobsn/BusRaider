
#include "sw_serial.h"
#include "nmalloc.h"    
#include "piwiring.h"
#include "timer.h"

typedef struct
{
    volatile unsigned int _putPos;
    volatile unsigned int _getPos;
    unsigned int _bufLen;
} RingBufferPosn;

void ringBufInit(RingBufferPosn* pRingBuf, int maxLen)
{
    pRingBuf->_bufLen = maxLen;
    pRingBuf->_putPos = 0;
    pRingBuf->_getPos = 0;
}

bool ringBufCanPut(RingBufferPosn* pRingBuf)
{
    if (pRingBuf->_bufLen == 0)
        return false;
    if (pRingBuf->_putPos == pRingBuf->_getPos)
        return true;
    unsigned int gp = pRingBuf->_getPos;
    if (pRingBuf->_putPos > gp)
    {
        if ((pRingBuf->_putPos != pRingBuf->_bufLen - 1) || (gp != 0))
            return true;
    }
    else
    {
        if (gp - pRingBuf->_putPos > 1)
            return true;
    }
    return false;
}

void ringBufHasPut(RingBufferPosn* pRingBuf)
{
    pRingBuf->_putPos++;
    if (pRingBuf->_putPos >= pRingBuf->_bufLen)
        pRingBuf->_putPos = 0;
}

bool ringBufCanGet(RingBufferPosn* pRingBuf)
{
    return pRingBuf->_putPos != pRingBuf->_getPos;
}


void ringBufHasGot(RingBufferPosn* pRingBuf)
{
    pRingBuf->_getPos++;
    if (pRingBuf->_getPos >= pRingBuf->_bufLen)
        pRingBuf->_getPos = 0;
}



typedef struct
{
    // Tx Timer
    // hw_timer_t *_txTimer = NULL;

    // Rx & Tx Pins
    int _swSerialTxPin;
    int _swSerialRxPin;

    // Serial Config
    int _nDataBits;
    int _nStopBits;

    // Tx Buffer
    uint8_t *_pTxBuffer;
    RingBufferPosn _txBufferPosn;

    // Tx progress
    uint8_t _txCurCh;
    int _txCurBit;

    // Rx Buffer
    uint8_t *_pRxBuffer;
    RingBufferPosn _rxBufferPosn;

    // Rx progress
    uint8_t _rxCurCh;
    int _rxCurBit;

    // Time in machine cycles since last edge
    unsigned long _rxLastEdgeCycleCount;

    // Baudrate
    int _baudRate;

    // Time of a bit duration (and half bit duration) in cycles
    unsigned long _rxBitTimeInCycles;
    unsigned long _rxHalfBitTimeInCycles;

} SwSerialContext;

static SwSerialContext swSerialContext;

// ISR for tx
static void onBitTimer()
{
    // See if already transmitting
    if (swSerialContext._txCurBit > 0)
    {
        // Send bit
        if (swSerialContext._txCurBit <= swSerialContext._nDataBits)
        {
            int nxtBitVal = swSerialContext._txCurCh >> (swSerialContext._txCurBit - 1);
            digitalWrite(swSerialContext._swSerialTxPin, (nxtBitVal & 0x01) != 0);
        }
        else
        {
            // Stop bits
            digitalWrite(swSerialContext._swSerialTxPin, 1);
        }
        // Next bit
        swSerialContext._txCurBit++;
        // Check if done
        if (swSerialContext._txCurBit >= 1 + swSerialContext._nDataBits + swSerialContext._nStopBits)
        {
            swSerialContext._txCurBit = 0;
        }
    }
    else
    {
        // Check for new data to send
        if (ringBufCanGet(&swSerialContext._txBufferPosn))
        {
            // Get char to send next
            swSerialContext._txCurCh = swSerialContext._pTxBuffer[swSerialContext._txBufferPosn._getPos];
            ringBufHasGot(&swSerialContext._txBufferPosn);

            // Send start bit
            digitalWrite(swSerialContext._swSerialTxPin, 0);
            swSerialContext._txCurBit = 1;
        }
    }

    // Check for no rx edge for several bit periods
    if (swSerialContext._rxCurBit > 0)
    {
        unsigned long curCycles = micros();
        // Time since last change
        unsigned long sinceLast = curCycles - swSerialContext._rxLastEdgeCycleCount;
        if (curCycles < swSerialContext._rxLastEdgeCycleCount)
            sinceLast = ULONG_MAX - swSerialContext._rxLastEdgeCycleCount + curCycles;
        // Check timeout
        int bitCount = (sinceLast + swSerialContext._rxHalfBitTimeInCycles) / swSerialContext._rxBitTimeInCycles;
        // If there hasn't been an edge for 2.5 times the max bits in a transmission
        // Then the stop bit didn't involve a transition and there hasn't been
        // another transmission started yet (to create a start bit edge)
        // 2.5 times is chosen so there isn't too much delay getting the received
        // data and to reduce the chance of a race condition where another byte
        // might start to be received while we're in this function
        if (bitCount > (swSerialContext._nDataBits * 5) / 2)
        {
            // Receive complete - fill the remaining spaces with 1s
            for (int i = 0; i < swSerialContext._nDataBits + 2 - swSerialContext._rxCurBit; i++)
                swSerialContext._rxCurCh = (swSerialContext._rxCurCh >> 1) + 0x80;

            // Add to received data only if there still hasn't been a start bit
            // received
            if (swSerialContext._rxCurBit > 0)
            {
                if (ringBufCanPut(&swSerialContext._rxBufferPosn))
                {
                    swSerialContext._pRxBuffer[swSerialContext._rxBufferPosn._putPos] = swSerialContext._rxCurCh;
                    ringBufHasPut(&swSerialContext._rxBufferPosn);
                }
                // Reset rx
                swSerialContext._rxCurCh = 0;
                swSerialContext._rxCurBit = 0;
            }
        }
    }
}

// ISR for Rx (change of pin value)
static void onRxChange()
{
    unsigned long curCycles = micros();
    // Time since last change
    unsigned long sinceLast = curCycles - swSerialContext._rxLastEdgeCycleCount;
    if (curCycles < swSerialContext._rxLastEdgeCycleCount)
        sinceLast = ULONG_MAX - swSerialContext._rxLastEdgeCycleCount + curCycles;
    int curLev = digitalRead(swSerialContext._swSerialRxPin);
    swSerialContext._rxLastEdgeCycleCount = micros();

    // Check the status of the edge
    if (swSerialContext._rxCurBit == 0)
    {
        if (curLev == 0)
        {
            // Start bit (no rx in progress)
            swSerialContext._rxCurCh = 0;
            swSerialContext._rxCurBit = 1;
        }
    }
    else
    {
        // Add the right number of bit of the opposite of the current value to the buffer
        int bitCount = (sinceLast + swSerialContext._rxHalfBitTimeInCycles) / swSerialContext._rxBitTimeInCycles;
        // // Check it is in a reasonable range
        if (bitCount > swSerialContext._nDataBits + 2 - swSerialContext._rxCurBit)
            bitCount = swSerialContext._nDataBits + 2 - swSerialContext._rxCurBit;
        // Move the bit position on
        swSerialContext._rxCurBit += bitCount;
        // Add the bits to the value - note that the start bit is initially shifted into
        // the byte and then this drops off the end
        while (bitCount > 0)
        {
            swSerialContext._rxCurCh = (swSerialContext._rxCurCh >> 1) + (curLev ? 0 : 0x80);
            bitCount--;
        }
        // Check if byte complete
        if (swSerialContext._rxCurBit > swSerialContext._nDataBits + 1)
        {
            // Add char to buffer
            if (ringBufCanPut(&swSerialContext._rxBufferPosn))
            {
                swSerialContext._pRxBuffer[swSerialContext._rxBufferPosn._putPos] = swSerialContext._rxCurCh;
                ringBufHasPut(&swSerialContext._rxBufferPosn);
            }
            swSerialContext._rxCurCh = 0;
            swSerialContext._rxCurBit = 0;
            // Check for start bit
            if (curLev == 0)
            {
                // Start bit
                swSerialContext._rxCurCh = 0;
                swSerialContext._rxCurBit = 1;
            }
        }
    }
}

void sw_serial_init(int txPin, int rxPin, int txBufferSize, int rxBufferSize)
{
    // Buffers
    ringBufInit(&swSerialContext._txBufferPosn, txBufferSize);
    swSerialContext._pTxBuffer = nmalloc_malloc(txBufferSize);
    ringBufInit(&swSerialContext._rxBufferPosn, rxBufferSize);
    swSerialContext._pRxBuffer = nmalloc_malloc(rxBufferSize);
    // Pins
    swSerialContext._swSerialTxPin = txPin;
    pinMode(txPin, OUTPUT);
    digitalWrite(txPin, HIGH);
    swSerialContext._swSerialRxPin = rxPin;
    pinMode(rxPin, INPUT);
}

void sw_serial_deinit()
{
    if (swSerialContext._pTxBuffer)
        nmalloc_free((void**)&swSerialContext._pTxBuffer);
    if (swSerialContext._pRxBuffer)
        nmalloc_free((void**)&swSerialContext._pRxBuffer);
}

void sw_serial_begin(long speed)
{
    swSerialContext._baudRate = speed;
    // Tx interrupt
    // int txTimerCount = 40000000 / speed;
    // _txTimer = timerBegin(0, 2, true);
    // timerAttachInterrupt(_txTimer, &onBitTimer, true);
    // timerAlarmWrite(_txTimer, txTimerCount, true);
    // timerAlarmEnable(_txTimer);
    // Rx interrupt
    swSerialContext._rxBitTimeInCycles = 1000000l / speed;
    swSerialContext._rxHalfBitTimeInCycles = swSerialContext._rxBitTimeInCycles / 2;
    attachInterrupt(digitalPinToInterrupt(_swSerialRxPin), onRxChange, CHANGE);
}

// virtual int read()
// {
//     if (!ringBufCanGet(&swSerialContext._rxBufferPosn))
//         return -1;
//     int ch = _pRxBuffer[_rxBufferPosn._getPos];
//     ringBufHasGot(&swSerialContext._rxBufferPosn)
//     return ch;
// }

//     virtual int peek()
//     {
//         if (!ringBufCanGet(&swSerialContext._rxBufferPosn))
//             return -1;
//         return _pRxBuffer[_rxBufferPosn._getPos];
//     }

//     virtual void flush()
//     {
//         _rxBufferPosn.clear();
//     }

//     virtual int available()
//     {
//         return _rxBufferPosn.count();
//     }

//     virtual size_t write(uint8_t ch)
//     {
//         if (!_txBufferPosn.canPut())
//             return 0;
//         _pTxBuffer[_txBufferPosn._putPos] = ch;
//         _txBufferPosn.hasPut();
//         return 1;
//     }

//     uint32_t getBitDurationInCycles()
//     {
//         return _rxBitTimeInCycles;
//     }

