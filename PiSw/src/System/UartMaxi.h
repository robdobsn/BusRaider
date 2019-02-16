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

private:
    // Rx Buffer & status
    uint8_t *_pRxBuffer;
    RingBufferPosn _rxBufferPosn;
    UART_ERROR_CODES _nRxStatus;

    // Tx Buffer
    uint8_t *_pTxBuffer;
    RingBufferPosn _txBufferPosn;

    static void isrStatic(void* pParam);
    void isr();

    int writeBase(unsigned int c);
    void txPumpPrime();
};
