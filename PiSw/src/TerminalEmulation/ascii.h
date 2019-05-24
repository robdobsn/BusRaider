/// \file ascii.h
///
/// Defines for ASCII characters
///
/// \date Nov 22, 2010
/// \author Mark Garlanger
///

#ifndef ASCII_H_
#define ASCII_H_

#include "stdint.h"

typedef uint8_t BYTE;

///
/// \namespace ascii
///
/// ASCII Constants.
///
namespace ascii
{
/// Null - not processed by the H19.
const BYTE NUL  = 0x00;

/// Start Of Heading - not processed by the H19.
const BYTE SOH  = 0x01;

/// Start Of Text - not processed by the H19.
const BYTE STX  = 0x02;

/// End Of Text - not processed by the H19.
const BYTE ETX  = 0x03;

/// End Of Transmission - not processed by the H19.
const BYTE EOT  = 0x04;

/// Enquiry - not processed by the H19.
const BYTE ENQ  = 0x05;

/// Acknowledge - not processed by the H19.
const BYTE ACK  = 0x06;

/// Bell - Rings the bell
const BYTE BEL  = 0x07;

/// Back Space
const BYTE BS   = 0x08;

/// Horizontal Tab
const BYTE HT   = 0x09;

/// Line Feed
const BYTE LF   = 0x0a;

/// Vertical Tab - not processed by the H19.
const BYTE VT   = 0x0b;

/// Form Feed - not processed by the H19.
const BYTE FF   = 0x0c;

/// Carriage Return
const BYTE CR   = 0x0d;

/// Shift Out - not processed by the H19.
const BYTE SO   = 0x0e;

/// Shift In - not processed by the H19.
const BYTE SI   = 0x0f;

/// Data Line Escape - not processed by the H19.
const BYTE DLE  = 0x10;

/// Device Control 1: Turns transmitter on (XON) - not processed by the H19.
const BYTE DC1  = 0x11;
const BYTE XON  = DC1;

/// Device Control 2 - not processed by the H19.
const BYTE DC2  = 0x12;

///  Device Control 3: Turns transmitter off (XOFF) - not processed by the H19.
const BYTE DC3  = 0x13;
const BYTE XOFF = DC3;

/// Device Control 4 - not processed by the H19.
const BYTE DC4  = 0x14;

/// Negative Acknowledge: Also ERR (error) - not processed by the H19.
const BYTE NAK  = 0x15;

/// Synchronous Idle - not processed by the H19.
const BYTE SYN  = 0x16;

/// End of Transmission Block - not processed by the H19.
const BYTE ETB  = 0x17;

/// Cancel. Cancels current Escape Sequence
const BYTE CAN  = 0x18;

/// End of Medium - not processed by the H19.
const BYTE EM   = 0x19;

/// Substitute - not processed by the H19.
const BYTE SUB  = 0x1a;

/// Escape
const BYTE ESC  = 0x1b;

/// File Separator - not processed by the H19.
const BYTE FS   = 0x1c;

/// Group Separator - not processed by the H19.
const BYTE GS   = 0x1d;

/// Record Separator - not processed by the H19.
const BYTE RS   = 0x1e;

/// Unit Separator - not processed by the H19.
const BYTE US   = 0x1f;

/// Space
const BYTE SP   = 0x20;

/// Delete (rubout) - not processed by the H19.
const BYTE DEL  = 0x7f;

}

#endif // ASCII_H_
