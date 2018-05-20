#ifndef _PIGFX_UART_H_
#define _PIGFX_UART_H_

#include "utils.h"

extern void uart_init(void);
extern unsigned int uart_poll();
extern void uart_purge();
extern void uart_write(const char* data, unsigned int size );
extern void uart_write_str(const char* data);
extern void uart_dump_mem(unsigned char* start_addr, unsigned char* end_addr);
extern void uart_load_ihex(void);
extern unsigned int uart_read_byte();
extern unsigned int uart_read_hex();
extern void uart_write_hex_u8 ( unsigned int d );
extern void uart_write_hex_u32 ( unsigned int d );


#endif
