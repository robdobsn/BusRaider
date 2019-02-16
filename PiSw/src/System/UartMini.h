// Bus Raider
// UART
// Rob Dobson 2018-2019

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void uart_init(unsigned int baudRate, bool use_interrupts);
extern void uart_init_irq();
extern unsigned int uart_poll();
extern void uart_purge();
extern void uart_send(unsigned int c);
extern void uart_write(const char* data, unsigned int size);
extern void uart_write_str(const char* data);

extern unsigned int uart_read_byte();

extern unsigned int uart_get_line_status(void);

extern volatile int globalUartCount;
extern volatile int globalUartDebug;

#ifdef __cplusplus
}
#endif
