#ifndef _PIBUSRAIDER_WIRING_H_
#define _PIBUSRAIDER_WIRING_H_

#include "utils.h"
#include "bare_metal_pi_zero.h"

extern uint8_t convModeToVal(uint8_t mode);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void digitalWrite(uint8_t pin, int val);

#endif
