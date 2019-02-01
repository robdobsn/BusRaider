#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <memory.h>

#pragma warning (disable : 4996)

#pragma pack(push, 1)
class Z80FileHeader
{
public:
	static const int Z80_HEADER_LENGTH = 30;
	static const int Z80_V2_LENGTH = 23;
	static const int Z80_V3_LENGTH = 54;
	static const int Z80_V3X_LENGTH = 55;

public:
	uint8_t A;
	uint8_t F;
	uint16_t BC;
	uint16_t HL;
	uint16_t PC;
	uint16_t SP;
	uint8_t InterruptRegister;
	uint8_t RefreshRegister;
	// ULA meaning
	// Bit 0  : Bit 7 of the R-register
	// Bit 1-3: Border colour
	// Bit 4  : 1=Basic SamRom switched in
	// Bit 5  : 1=Block of data is compressed
	// Bit 6-7: No meaning
	uint8_t Flags1;
	uint16_t DE;
	uint16_t BC_Dash;
	uint16_t DE_Dash;
	uint16_t HL_Dash;
	uint8_t A_Dash;
	uint8_t F_Dash;
	uint16_t IY;
	uint16_t IX;
	uint8_t IFF1;
	uint8_t IFF2;
	uint8_t Flags2;

	uint8_t dummyValueToIndicateEndOfStructOnDisk;

	uint8_t ULA;
	uint8_t IM;
	uint8_t Issue2;

	int _posOfDataStart;
	int _isCompressed;
	int _version;

	uint8_t* _pRawRegValues;
	int _rawRegValuesLen;

public:
	void swapEndian16Bit(uint16_t& val)
	{
		val = (val >> 8) | (val << 8);
	}

	uint16_t getInt16(const uint8_t* pBuf)
	{
		uint16_t val = *pBuf + (*(pBuf + 1) << 8);
		swapEndian16Bit(val);
		return val;
	}

	bool fillFromBuffer(const uint8_t* pBuf, int offset, int len)
	{
		// Handle read
		int sizeToRead = (&dummyValueToIndicateEndOfStructOnDisk) - (&A);
		//ASSERT(sizeToRead == Z80_HEADER_LENGTH);
		if (len < offset + sizeToRead)
			return false;
		memcpy(this, pBuf + offset, sizeToRead);

		// Comment in https://www.worldofspectrum.org/faq/reference/z80format.htm
		// indicates that the following is needed
		if (Flags1 == 255)
			Flags1 = 1;

		// Fix-up the refresh register, etc
		RefreshRegister = (RefreshRegister & 0x7f) | ((Flags1 & 0x01) << 7);
		ULA = (Flags1 & 0x0e) >> 1;
		IFF1 = IFF1 ? 1 : 0;
		IFF2 = IFF2 ? 1 : 0;
		IM = Flags2 & 0x03;
		Issue2 = Flags2 & 0x04;

		// Check which endian-ness
		uint16_t testVal = 1;
		bool bigEndian = (*((uint8_t*)&testVal)) == 0;
		if (bigEndian)
		{
			// Need to swap two byte values
			swapEndian16Bit(BC);
			swapEndian16Bit(HL);
			swapEndian16Bit(PC);
			swapEndian16Bit(SP);
			swapEndian16Bit(DE);
			swapEndian16Bit(BC_Dash);
			swapEndian16Bit(DE_Dash);
			swapEndian16Bit(HL_Dash);
			swapEndian16Bit(IX);
			swapEndian16Bit(IY);
		}

		// Handle versions
		if (PC == 0)
		{
			// V2 or greater
			size_t extraLen = pBuf[sizeToRead] + (pBuf[sizeToRead + 1] << 8);
			if (extraLen == Z80_V2_LENGTH)
				_version = 2;
			else if (extraLen == Z80_V3_LENGTH)
				_version = 3;
			else if (extraLen == Z80_V3X_LENGTH)
				_version = 3;
			else
				_version = 0;

			// Get PC
			PC = getInt16(pBuf + Z80_HEADER_LENGTH + 2);

			// Start of data
			_posOfDataStart = Z80_HEADER_LENGTH + 2 + extraLen;
			_isCompressed = false; // this is set later from the block header
		}
		else
		{
			_posOfDataStart = Z80_HEADER_LENGTH;
			_isCompressed = (Flags1 & 0x20) != 0;
			_version = 1;
		}

		// Values
		_pRawRegValues = &A;
		_rawRegValuesLen = sizeToRead;
	}

};
#pragma pack(pop)

class Z80DataBlock
{
public:
	static const int V1_BLOCK_SIZE = 48 * 1024;
	static const int V2_BLOCK_SIZE = 16 * 1024;
	uint8_t* _pData;
	int _len;
	bool _isCompressed;
	int _version;
	int _nextBlockStartPos;
	int _pageNumber;

	Z80DataBlock()
	{
		_pData = NULL;
		_len = 0;
		_isCompressed = 0;
		_version = 0;
		_nextBlockStartPos = 0;
		_pageNumber = -1;
	}

	~Z80DataBlock()
	{
		if (_pData)
			delete[] _pData;
	}

	bool readV1(uint8_t* pBuf, int& remainingLen)
	{
		// Allocate 48K as each block must be that length
		if (_pData)
			delete[] _pData;
		_pData = new uint8_t[V1_BLOCK_SIZE];
		_pageNumber = 0;

		// Check compressed
		uint8_t* pCur = pBuf;
		uint8_t* pEnd = pCur + remainingLen;
		int outPos = 0;
		if (_isCompressed)
		{
			// Loop through the data
			while (pEnd > pCur)
			{
				// Check for escape
				if ((pEnd-pCur >= 4) && (*pCur == 0xed) && (*(pCur + 1) == 0xed))
				{
					int len = *(pCur + 2);
					int byte = *(pCur + 3);
					for (int j = 0; j < len; j++)
						_pData[outPos++] = byte;
					pCur += 4;
				}
				else if (outPos < V1_BLOCK_SIZE)
				{
					_pData[outPos++] = *pCur++;
				}
				if (outPos >= V1_BLOCK_SIZE)
				{
					_len = V1_BLOCK_SIZE;
					_nextBlockStartPos = pCur - pBuf + 4;
					remainingLen -= _nextBlockStartPos;
					return true;
				}
			}
			printf("read ran out of file\n");
			return false;
		}
		else
		{
			// Not compressed
			if (remainingLen < V1_BLOCK_SIZE)
			{
				printf("read_v1_block: not enough data in buffer\n");
				_len = 0;
				return false;
			}

			// Copy the block
			memcpy(_pData, pBuf, V1_BLOCK_SIZE);
			_len = V1_BLOCK_SIZE;
			_nextBlockStartPos = V1_BLOCK_SIZE;
			remainingLen -= _nextBlockStartPos;
			return true;
		}
	}

	bool readV2(uint8_t* pBuf, int& remainingLen)
	{
		// Allocate 16K as each block must be that length
		if (_pData)
			delete[] _pData;
		_pData = new uint8_t[V2_BLOCK_SIZE];

		// Get length and compression
		int compressedLen = pBuf[0] +(pBuf[1] << 8);
		_isCompressed = compressedLen != 0xffff;
		_pageNumber = pBuf[2];

		// Get block
		uint8_t* pCur = pBuf+3;
		uint8_t* pEnd = pCur + compressedLen;
		int outPos = 0;
		if (_isCompressed)
		{
			// Loop through the data
			while (pEnd > pCur)
			{
				// Check for escape
				if ((pEnd - pCur >= 4) && (*pCur == 0xed) && (*(pCur + 1) == 0xed))
				{
					int len = *(pCur + 2);
					int byte = *(pCur + 3);
					for (int j = 0; j < len; j++)
						_pData[outPos++] = byte;
					pCur += 4;
				}
				else if (outPos < V2_BLOCK_SIZE)
				{
					_pData[outPos++] = *pCur++;
				}
				if (outPos >= V2_BLOCK_SIZE)
				{
					_len = V2_BLOCK_SIZE;
					_nextBlockStartPos = compressedLen + 3;
					remainingLen -= _nextBlockStartPos;
					return true;
				}
			}
			printf("read ran out of file\n");
			return false;
		}
		else
		{
			// Not compressed
			if (remainingLen - 3 < V2_BLOCK_SIZE)
			{
				printf("read_v1_block: not enough data in buffer\n");
				_len = 0;
				return false;
			}

			// Copy the block
			memcpy(_pData, pBuf+3, V2_BLOCK_SIZE);
			_len = V2_BLOCK_SIZE;
			_nextBlockStartPos = V2_BLOCK_SIZE + 3;
			remainingLen -= _nextBlockStartPos;
			return true;
		}
	}

	bool read(uint8_t* pBuf, int& remainingLen, int version, bool isCompressed)
	{
		_version = version;
		_isCompressed = isCompressed;
		// Read
		if (version == 1)
			return readV1(pBuf, remainingLen);
		return readV2(pBuf, remainingLen);
	}
};

bool interpFile(uint8_t* pBuf, int len)
{
	// Read header
	Z80FileHeader fileHeader;
	if (!fileHeader.fillFromBuffer(pBuf, 0, len))
	{
		return false;
	}

	// Now read blocks
	uint8_t* pBlockPtr = pBuf + fileHeader._posOfDataStart;
	int remainingLen = len - fileHeader._posOfDataStart;

	// Block List
	static const int MAX_PAGES = 12;
	Z80DataBlock* dataBlocks[MAX_PAGES];
	for (int i = 0; i < MAX_PAGES; i++)
		dataBlocks[i] = 0;

	// Loop over blocks
	while (remainingLen > 0)
	{
		// Read the block
		Z80DataBlock* pDataBlock = new Z80DataBlock();
		if (!pDataBlock->read(pBlockPtr, remainingLen, fileHeader._version, fileHeader._isCompressed))
		{
			printf("Read failed\n");
			return false;
		}
		pBlockPtr += pDataBlock->_nextBlockStartPos;

		// Store in list
		if ((pDataBlock->_pageNumber > 0) && (pDataBlock->_pageNumber < MAX_PAGES))
			dataBlocks[pDataBlock->_pageNumber] = pDataBlock;
		printf("Read block page num %d len %d\n", pDataBlock->_pageNumber, pDataBlock->_len);
	}

	// Assemble the image
	const int FULL_MEM_SIZE = 0x10000;
	uint8_t* pFullMem = new uint8_t[FULL_MEM_SIZE];
	memset(pFullMem, 0, FULL_MEM_SIZE);

	// Read spectrum ROM
	FILE *fRom = fopen("48.rom", "rb");
	fread(pFullMem, 1, FULL_MEM_SIZE, fRom);
	fclose(fRom);

	// Check version of format
	//if ((fileHeader._version == 1) && (dataBlocks[0]))
	//{
	//	// Single 48K block
	//	memcpy(pFullMem + 0x4000, dataBlocks[0]->_pData, dataBlocks[0]->_len);
	//}
	//else
	//{
	//	// Separate 16K blocks, one for each page
	//	// https://www.worldofspectrum.org/faq/reference/z80format.htm
	//	// We're interested in pages 8 (=0x4000), 4 = (0x8000), 5 (=0xc000) and 0 (=0x0000 if it exists)
	//	if (dataBlocks[8])
	//		memcpy(pFullMem + 0x4000, dataBlocks[8]->_pData, dataBlocks[8]->_len);
	//	if (dataBlocks[4])
	//		memcpy(pFullMem + 0x8000, dataBlocks[4]->_pData, dataBlocks[4]->_len);
	//	if (dataBlocks[5])
	//		memcpy(pFullMem + 0xc000, dataBlocks[5]->_pData, dataBlocks[5]->_len);
	//}

	//// Jump to setup function
	//int memPtr = 0;
	//pFullMem[memPtr++] = 0xc3;
	//pFullMem[memPtr++] = fileHeader.PC & 0xff;
	//pFullMem[memPtr++] = fileHeader.PC >> 8;

	////pFullMem[memPtr++] = 0x31;
	////pFullMem[memPtr++] = fileHeader.SP & 0xff;
	////pFullMem[memPtr++] = fileHeader.SP >> 8;

	// Insert jump to 0x4000 - screen RAM
	int memPtr = 0;
	pFullMem[memPtr++] = 0xc3;
	pFullMem[memPtr++] = 0;
	pFullMem[memPtr++] = 0x80;

	// Code to reload registers
	static const uint8_t reloadCode[] =
	{
		0x3e, 0x21, 0x21, 0x00, 0x40, 0x11, 0x01, 0x40, 0x77, 0x01, 0x00, 0x10, 0xed, 0xb0, 0x3c, 0xfe,
		0x7e, 0xc2, 0x02, 0x80, 0xc3, 0x00, 0x80,

		//0x3e, 0x55, 0x3e, 0x02, 0xcd, 0x01, 0x16, 0x11, 0x13, 0x40, 0x01, 0x06, 0x00, 0xcd, 0x3c, 0x20,
		//0xc3, 0x07, 0x40, 0x48, 0x65, 0x72, 0x65, 0x72, 0x65,

		0xdd, 0x21, 0x00, 0x42, 0xdd, 0x4e, 0x0f, 0xdd, 0x46, 0x10, 0xdd, 0x5e, 0x11, 0xdd, 0x56, 0x12, 
		0xdd, 0x6e, 0x13, 0xdd, 0x66, 0x14, 0xd9, 0x31, 0x17, 0x42, 0xf1, 0x08, 0xdd, 0x7e, 0x0a, 0xed, 
		0x47, 0xdd, 0x7e, 0x0b, 0xed, 0x4f, 0x31, 0x1b, 0x42, 0xdd, 0xe1, 0xfd, 0xe1, 0x31, 0x06, 0x42, 
		0xe1, 0xc1, 0xf1, 0xed, 0x5b, 0x0d, 0x42, 0xed, 0x7b, 0x08, 0x42, 0x2a, 0x06, 0x42, 0xe5, 0x2a, 
		0x04, 0x42, 0xc9
	};

	// Copy the code
	memcpy(pFullMem + 0x8000, reloadCode, sizeof(reloadCode));

	// Copy the code
	memcpy(pFullMem + 0x4200, fileHeader._pRawRegValues, fileHeader._rawRegValuesLen);

	const char* outFileName = "z80_64K.bin";
	FILE *fp = fopen(outFileName, "wb");
	fwrite(pFullMem, 1, FULL_MEM_SIZE, fp);
	fclose(fp);

	printf("Written file %s\n", outFileName);

	return true;
}

int main(int argc, char** argv)
{
	printf("Test reading Z80 Snapshot\n");

	const char *fileName = "./printhello.z80";
	// const char *fileName = "./test.z80";
	//	const char *fileName = "./FuseSpectrumStartSnapshot.z80";

	// Read the file into memory
	FILE *fp = fopen(fileName, "rb");
	if (!fp)
	{
		printf("Cannot open file %s\n", fileName);
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* pFileContents = new uint8_t[filesize];
	fread(pFileContents, filesize, 1, fp);
	fclose(fp);

	// Interpret the file
	interpFile(pFileContents, filesize);

	// Clean up
	delete[] pFileContents;

	// Wait
	getchar();
}