// Bus Raider
// Rob Dobson 2018

#include "McTRS80CmdFormat.h"
#include "../System/logging.h"
#include <string.h>
#include <stdint.h>

const char *McTRS80CmdFormat::_logPrefix = "McTRS80Cmd";

McTRS80CmdFormat::McTRS80CmdFormat()
{
}

void McTRS80CmdFormat::proc(FileParserDataCallback* pDataCallback, 
            FileParserRegsCallback* pRegsCallback, 
            const uint8_t* pData, int dataLen)
{
    int pos = 0;
    while (pos < dataLen - 2)
    {
        // Record start info
        int code = pData[pos++];
        int length = pData[pos++];
        length = (length < dataLen-1) ? length : (dataLen-1);
        if (length == 0)
            length = 0x100;

        // Code
        switch(code)
        {
            case 1:
            {
                // Data segment
                uint32_t addr = pData[pos] + (((uint32_t)pData[pos+1]) << 8);
                pos+=2;
                if (length < 3)
                    length += 0xfe;
                else
                    length -= 2;
                pDataCallback(addr, pData+pos, length);
                LogWrite(_logPrefix, LOG_DEBUG, "Code segment addr %04x len %04x", addr, length);
                break;
            }
            case 2:
            {
                // Exec addr
                if (length == 1)
                {
                    Z80Registers regs;
                    regs.PC = pData[pos];
                    pRegsCallback(regs);
                    LogWrite(_logPrefix, LOG_DEBUG, "ExecAddr8 %04x", regs.PC);
                }
                else if (length == 2)
                {
                    Z80Registers regs;
                    regs.PC = pData[pos] + (((uint32_t)pData[pos+1]) << 8);
                    pos+=2;
                    pRegsCallback(regs);
                    LogWrite(_logPrefix, LOG_DEBUG, "ExecAddr16 %04x", regs.PC);
                }
                else
                {
                    LogWrite(_logPrefix, LOG_DEBUG, "Error in exec addr");
                }
                break;
            }
            case 3:
            {
                // Non exec marker
                LogWrite(_logPrefix, LOG_DEBUG, "Non exec marker");
                return;
            }
            case 4:
            {
                LogWrite(_logPrefix, LOG_DEBUG, "End of partitioned data");
                break;
            }
            case 5:
            {
                LogWrite(_logPrefix, LOG_DEBUG, "Title");
                break;
            }
            default:
            {
                LogWrite(_logPrefix, LOG_DEBUG, "Undecoded block %02x", code);
                break;
            }
        }

        // Bump pos
        pos += length;
    }
}
