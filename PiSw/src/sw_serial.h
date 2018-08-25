// Bus Raider
// Rob Dobson 2018

#pragma once

#include "utils.h"

extern void sw_serial_init(int txPin, int rxPin, int txBufferSize, int rxBufferSize);
extern void sw_serial_deinit();
extern void sw_serial_begin(long speed);

// extern unsigned int sw_serial_poll();
// extern void sw_serial_purge();
// extern void sw_serial_send(unsigned int c);
// extern void sw_serial_write(const char* data, unsigned int size);
// extern void sw_serial_write_str(const char* data);
// extern void sw_serial_dump_mem(unsigned char* start_addr, unsigned char* end_addr);
// extern void sw_serial_load_ihex(void);
// extern unsigned int sw_serial_read_byte();
// extern unsigned int sw_serial_read_hex();
// extern void sw_serial_write_hex_u8(unsigned int d);
// extern void sw_serial_write_hex_u32(unsigned int d);
