// Bus Raider
// UartMini
// Rob Dobson 2018-2019

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

class UartMini
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
    UartMini();
    ~UartMini();

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
    UART_ERROR_CODES _nRxStatus;

    // Rx Buffer
    static const int RXBUFMASK = 0x7FFF;
    static volatile unsigned int rxhead;
    static volatile unsigned int rxtail;
    static volatile unsigned char rxbuffer[RXBUFMASK + 1];

    static void isrStatic(void* pParam);
    void isr();
};



// // Bus Raider
// // UART
// // Rob Dobson 2018-2019

// #pragma once

// #include <stdbool.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

// extern void uart_init(unsigned int baudRate, bool use_interrupts);
// extern void uart_init_irq();
// extern unsigned int uart_poll();
// extern void uart_purge();
// extern void uart_send(unsigned int c);
// extern void uart_write(const char* data, unsigned int size);
// extern void uart_write_str(const char* data);

// extern unsigned int uart_read_byte();

// extern unsigned int uart_get_line_status(void);

// extern volatile int globalUartCount;
// extern volatile int globalUartDebug;

// #ifdef __cplusplus
// }
// #endif
