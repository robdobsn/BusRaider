// Bus Raider
// Rob Dobson 2020

#include "logging.h"
#include "McZXSpectrumTZXFormat.h"

static const char* MODULE_PREFIX = "McSpecTZX";

McZXSpectrumTZXFormat::McZXSpectrumTZXFormat()
{
    _curHeaderType = 0;
    _curDataLen = 0;
    _curAutostartLine = 0;
    _curProgramLen = 0;
    _curVarName = 0;
    // This is the address basic programs load to I think
    _curStartAddr = 0x5ccb;
}

void McZXSpectrumTZXFormat::getString(const uint8_t* pData, int dataLen,
            int recPos, int strLen, char* pDest)
{
    int maxLen = strLen < (dataLen - recPos) ? strLen : (dataLen - recPos);
    strlcpy(pDest, (const char*) (pData+recPos), maxLen+1);
    pDest[maxLen] = 0;
}

uint32_t McZXSpectrumTZXFormat::getWord16(const uint8_t* pData, int dataLen, int recPos)
{
    if (dataLen < recPos + 2)
        return 0;
    return ((uint32_t)(pData[recPos])) + ((uint32_t)(pData[recPos+1]) << 8);
}

uint32_t McZXSpectrumTZXFormat::getByte(const uint8_t* pData, int dataLen, int recPos)
{
    if (dataLen < recPos + 1)
        return 0;
    return pData[recPos];
}

// Info from http://www.zx-modules.de/fileformats/tapformat.html
bool McZXSpectrumTZXFormat::decodeTapBlock(FileParserDataCallback* pDataCallback,
     FileParserRegsCallback* pRegsCallback,
    void* pCallbackParam,
    const uint8_t* pData, int curOffset, int totalLen, int dataLen)
{
    // Handle headers
    int flagByte = pData[curOffset];
    if (flagByte == 0x00)
    {
        _curHeaderType = getByte(pData, totalLen, curOffset+1);
        // Which header type
        if (_curHeaderType == 0x00)
        {
            // program header (basic language)
            char fileName[11];
            getString(pData, totalLen, curOffset + 2, 10, fileName);
            _curDataLen = getWord16(pData, totalLen, curOffset + 12);
            _curAutostartLine = getWord16(pData, totalLen, curOffset + 14);
            _curProgramLen = getWord16(pData, totalLen, curOffset + 16);
            int checksum = getByte(pData, totalLen, curOffset + 18);
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "ProgramHeader %s dataLen %d autostartLine %d programLen %d checksum %02x",
                fileName, _curDataLen, _curAutostartLine, _curProgramLen, checksum);
        }
        else if (_curHeaderType == 0x01)
        {
            // numeric array header
            char fileName[11];
            getString(pData, totalLen, curOffset + 2, 10, fileName);
            _curDataLen = getWord16(pData, totalLen, curOffset + 12);
            _curVarName = getByte(pData, totalLen, curOffset + 15);
            int checksum = getByte(pData, totalLen, curOffset + 17);
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "NumArrayHeader %s dataLen %d varName %d checksum %02x",
                fileName, _curDataLen, _curVarName, checksum);
        }
        else if (_curHeaderType == 0x02)
        {
            // string array header
            char fileName[11];
            getString(pData, totalLen, curOffset + 2, 10, fileName);
            _curDataLen = getWord16(pData, totalLen, curOffset + 12);
            _curVarName = getByte(pData, totalLen, curOffset + 15);
            int checksum = getByte(pData, totalLen, curOffset + 17);
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "StringArrayHeader %s dataLen %d varName %d checksum %02x",
                fileName, _curDataLen, _curVarName, checksum);
        }
        else if (_curHeaderType == 0x03)
        {
            // byte header
            char fileName[11];
            getString(pData, totalLen, curOffset + 2, 10, fileName);
            _curDataLen = getWord16(pData, totalLen, curOffset + 12);
            _curStartAddr = getWord16(pData, totalLen, curOffset + 14);
            int checksum = getByte(pData, totalLen, curOffset + 17);
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "ByteHeader %s dataLen %d startAddr %04x checksum %02x",
                fileName, _curDataLen, _curStartAddr, checksum);
        }
        else
        {
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "UnknownHeader %02x", _curHeaderType);
        }
    }
    else if (flagByte == 0xff)
    {
        // Data block
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "DataBlock len %d", dataLen);
        pDataCallback(_curStartAddr, pData + curOffset + 1, dataLen, pCallbackParam);
    }
    else
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "UnknownFlagType %02x", flagByte);
    }
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "\n");
    return true;
}

bool McZXSpectrumTZXFormat::proc(FileParserDataCallback* pDataCallback, 
             FileParserRegsCallback* pRegsCallback, 
            void* pCallbackParam,
            const uint8_t* pData, int dataLen)
{
    // Check format is valid
    if (strncmp((const char*)pData, "ZXTape!", 7) != 0)
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Invalid format\n");
        return false;
    }

    // Handle the file record by record
    int recPos = 0x0a;
    while(recPos < dataLen)
    {
        int recType = pData[recPos++];
        int recLen = 0;
        switch(recType)
        {
            case 0x10:
            {
                int nDelay = getWord16(pData, dataLen, recPos);
                recLen = getWord16(pData, dataLen, recPos+2);
                //LogWrite(MODULE_PREFIX, LOG_DEBUG, "Std speed data, delay %dms len %d\n", nDelay, recLen);
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Std speed data, delay %dms len %d => ", nDelay, recLen);
                decodeTapBlock(pDataCallback, pRegsCallback, pCallbackParam, pData, recPos + 4, dataLen, recLen);
                recPos += recLen + 4;
                break;
            }
            case 0x30:
            {
                recLen = getByte(pData, dataLen, recPos);
                char descrString[MAX_DESCR_LEN+1];
                getString(pData, dataLen, recPos+1, recLen, descrString);
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Text description, len %d = %s\n", recLen, descrString);
                recPos += recLen + 1;
                break;
            }
        }
    }
    return true;
}