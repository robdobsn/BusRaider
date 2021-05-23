/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileBlockInfo
// Collection of information about a block in a file
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>

class FileBlockInfo
{
public:
    FileBlockInfo()
    {
        filename = NULL;
        contentLen = 0;
        filePos = 0;
        pBlock = NULL;
        blockLen = 0;
        finalBlock = false;
        crc16 = 0;
        crc16Valid = false;
        fileLen = 0;
        fileLenValid = false;
    }
    FileBlockInfo(const char* filename,
        uint32_t contentLen,
        uint32_t filePos,
        const uint8_t* pBlock,
        uint32_t blockLen,
        bool finalBlock,
        uint32_t crc16,
        bool crc16Valid,
        uint32_t fileLen,
        bool fileLenValid
    )
    {
        this->filename = filename;
        this->contentLen = contentLen;
        this->filePos = filePos;
        this->pBlock = pBlock;
        this->blockLen = blockLen;
        this->finalBlock = finalBlock;
        this->crc16 = crc16;
        this->crc16Valid = crc16Valid;
        this->fileLen = fileLen;
        this->fileLenValid = fileLenValid;
    }
    const char* filename;
    uint32_t contentLen;
    uint32_t filePos;
    const uint8_t* pBlock;
    uint32_t blockLen;
    bool finalBlock;
    uint32_t crc16;
    bool crc16Valid;
    uint32_t fileLen;
    bool fileLenValid;
};