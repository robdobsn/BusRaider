// ESP32 Software Serial using Interrupts for Rx and Tx
// Rob Dobson 2018

#include "RdSwSerial.h"

// Hardware pins
int SwSerial::_swSerialTxPin = -1;
int SwSerial::_swSerialRxPin = -1;

// Buffers
uint8_t* SwSerial::_pTxBuffer = NULL;
RingBufferPosn SwSerial::_txBufferPosn(0);
uint8_t* SwSerial::_pRxBuffer = NULL;
RingBufferPosn SwSerial::_rxBufferPosn(0);

// Current character tx and rx
int SwSerial::_txCurBit = 0;
uint8_t SwSerial::_txCurCh = 0;
int SwSerial::_rxCurBit = 0;
uint8_t SwSerial::_rxCurCh = 0;

// Time in machine cycles since last edge
unsigned long SwSerial::_rxLastEdgeCycleCount = 0;

// Time of a bit (and half bit) duration in cycles - set elsewhere
unsigned long SwSerial::_rxBitTimeInCycles = 1;
unsigned long SwSerial::_rxHalfBitTimeInCycles = 1;

// Serial properties
int SwSerial::_nDataBits = 8;
int SwSerial::_nStopBits = 1;
