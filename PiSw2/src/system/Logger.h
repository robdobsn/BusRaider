/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Log
//
// BusRaider Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "logging.h"

#define LOG_E( tag, format, ... ) LogWrite(tag, LOG_ERROR, format, ##__VA_ARGS__);
#define LOG_W( tag, format, ... ) LogWrite(tag, LOG_WARNING, format, ##__VA_ARGS__ )
#define LOG_I( tag, format, ... ) LogWrite(tag, LOG_NOTICE, format, ##__VA_ARGS__ )
#define LOG_D( tag, format, ... ) LogWrite(tag, LOG_DEBUG, format, ##__VA_ARGS__ )
#define LOG_V( tag, format, ... ) LogWrite(tag, LOG_VERBOSE, format, ##__VA_ARGS__ )

