// Bus Raider
// Rob Dobson 2018

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bare_metal_pi_zero.h"
#include "utils.h"
#include "rdutils.h"

extern uint8_t convModeToVal(uint8_t mode);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void digitalWrite(uint8_t pin, int val);
extern int digitalRead(uint8_t pin);

#ifdef __cplusplus
}
#endif
