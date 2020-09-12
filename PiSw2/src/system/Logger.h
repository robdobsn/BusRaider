/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Log
//
// BusRaider Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <circle/logger.h>

#define LOG_E( tag, format, ... ) CLogger::Get()->Write( tag, LogError, format, ##__VA_ARGS__ )
#define LOG_W( tag, format, ... ) CLogger::Get()->Write( tag, LogWarning, format, ##__VA_ARGS__ )
#define LOG_I( tag, format, ... ) CLogger::Get()->Write( tag, LogNotice, format, ##__VA_ARGS__ )
#define LOG_D( tag, format, ... ) CLogger::Get()->Write( tag, LogDebug, format, ##__VA_ARGS__ )
#define LOG_V( tag, format, ... ) CLogger::Get()->Write( tag, LogDebug, format, ##__VA_ARGS__ )

