// ArduinoTime
// Rob Dobson 2012-2021

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// From arduino-esp32
uint64_t micros();
unsigned long millis();
void delay(uint32_t);
void delayMicroseconds(uint64_t us);

#ifdef __cplusplus
}
#endif
