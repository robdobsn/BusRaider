// Bus Raider
// UartMaxi
// Rob Dobson 2018-2019

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "RingBufferPosn.h"

class UartMaxi
{
public:
    enum UART_ERROR_CODES
    {
        UART_ERROR_NONE,
        UART_ERROR_BREAK,
        UART_ERROR_OVERRUN,
        UART_ERROR_FRAMING,
        UART_ERROR_FULL
    };
public:
    UartMaxi();
    ~UartMaxi();

    // Setup and clear
    bool setup(unsigned int baudRate, int rxBufSize, int txBufSize);
    void clear();

    // Output
    int txAvailable();
    int write(unsigned int c);
    void write(const char* data, unsigned int size);
    void writeStr(const char* data);

    // Input
    int read();
    int available();
    bool poll();
    int peek();
    UART_ERROR_CODES getStatus()
    {
        UART_ERROR_CODES tmpCode = _nRxStatus;
        _nRxStatus = UART_ERROR_NONE;
        return tmpCode;
    }
    void getStatusCounts(int& txBufferFullCount, int& rxFramingCount, int& rxOverrunCount, 
                int& rxBreakCount, int &rxBufferFullCount)
    {
        txBufferFullCount = _txBufferFullCount;
        rxFramingCount = _rxFramingErrCount;
        rxOverrunCount = _rxOverrunErrCount;
        rxBreakCount = _rxBreakCount;
        rxBufferFullCount = _rxBufferFullCount;
    }
    void resetStatusCounts()
    {
        _txBufferFullCount = 0;
        _rxFramingErrCount = 0;
        _rxOverrunErrCount = 0;
        _rxBreakCount = 0;
        _rxBufferFullCount = 0;
    }

private:
    // Rx Buffer & status
    uint8_t *_pRxBuffer;
    RingBufferPosn _rxBufferPosn;
    UART_ERROR_CODES _nRxStatus;
    int _rxFramingErrCount;
    int _rxOverrunErrCount;
    int _rxBreakCount;
    int _rxBufferFullCount;

    // Tx Buffer
    uint8_t *_pTxBuffer;
    RingBufferPosn _txBufferPosn;
    int _txBufferFullCount;

    static void isrStatic(void* pParam);
    void isr();

    int writeBase(unsigned int c);
    void txPumpPrime();
};
