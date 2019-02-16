// Bus Raider
// UART
// Rob Dobson 2018-2019

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "RingBufferPosn.h"

class UartMaxi
{
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
    unsigned int poll();
    int peek();

private:
    static const int RX_BUF_LEN = 100000;

    // Rx Buffer
    uint8_t *_pRxBuffer;
    RingBufferPosn _rxBufferPosn;

    // Tx Buffer
    uint8_t *_pTxBuffer;
    RingBufferPosn _txBufferPosn;

    static void isrStatic(void* pParam);
    void isr();
};
