// Bus Raider Wiring Library (like Arduino)
// Rob Dobson 2018

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3
#define PINMODE_ALT0 0x4
#define PINMODE_ALT1 0x5
#define PINMODE_ALT2 0x6
#define PINMODE_ALT3 0x7
#define PINMODE_ALT4 0x8
#define PINMODE_ALT5 0x9

extern uint8_t convModeToVal(uint8_t mode);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void digitalWrite(uint8_t pin, int val);
extern int digitalRead(uint8_t pin);

#ifdef __cplusplus
}
#endif
