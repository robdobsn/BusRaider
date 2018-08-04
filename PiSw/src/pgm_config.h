#ifndef _PIGFX_CONFIG_H_
#define _PIGFX_CONFIG_H_

#include "globaldefs.h"

#define PGM_VERSION             "V1.3"
#define SEND_CR_LF              ON              /* Send CR+LF when Return is pressed */
#define BACKSPACE_ECHO          OFF             /* Auto-echo the backspace char */
#define SWAP_DEL_WITH_BACKSPACE ON              /* Substitute DEL (0x7F) with BACKSPACE (0x08) */
#define SKIP_BACKSPACE_ECHO     OFF             /* Skip the next incoming character after a backspace from keyboard */
#define HEARTBEAT_FREQUENCY     1               /* Status led blink frequency in Hz */

// Log framebuffer operations to UART
#define FRAMEBUFFER_DEBUG       1             
// Log mailbox operations to UART
//#define POSTMAN_DEBUG           1
// Graphics functions use DMA when possible (faster, but occupy DMA channel 0)             
#define GFX_USE_DMA             1

#endif
