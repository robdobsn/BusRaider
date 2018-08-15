// Bus Raider
// Rob Dobson 2018

#include "mc_trs80_cmdfile.h"
#include "ee_printf.h"

static const char FromTrs80CmdFile[] = "TRS80CmdFile";

void mc_trs80_cmdfile_init()
{

}

void mc_trs80_cmdfile_proc(CmdFileParserDataCallback* pDataCallback, CmdFileParserAddrCallback* pAddrCallback, const uint8_t* pData, int dataLen)
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
                LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: Code segment addr %04x len %04x\n", addr, length);
                break;
            }
            case 2:
            {
                // Exec addr
                uint32_t execAddr = 0;
                if (length == 1)
                {
                    execAddr = pData[pos];
                    pAddrCallback(execAddr);
                    LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: ExecAddr8 %04x", execAddr);
                }
                else if (length == 2)
                {
                    execAddr = pData[pos] + (((uint32_t)pData[pos+1]) << 8);
                    pos+=2;
                    pAddrCallback(execAddr);
                    LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: ExecAddr16 %04x\n", execAddr);
                }
                else
                {
                    LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: Error in exec addr\n");
                }
                break;
            }
            case 3:
            {
                // Non exec marker
                LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: Non exec marker\n");
                return;
            }
            case 4:
            {
                LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: End of partitioned data\n");
                break;
            }
            case 5:
            {
                LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: Title\n");
                break;
            }
            default:
            {
                LogWrite(FromTrs80CmdFile, LOG_DEBUG, "CmdFile: Undecoded block %02x\n", code);
                break;
            }
        }

        // Bump pos
        pos += length;
    }
}
