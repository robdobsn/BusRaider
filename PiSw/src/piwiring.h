// Bus Raider Wiring Library (like Arduino)
// Rob Dobson 2018

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t convModeToVal(uint8_t mode);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void digitalWrite(uint8_t pin, int val);
extern int digitalRead(uint8_t pin);

#ifdef __cplusplus
}
#endif
