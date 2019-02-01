
#include "../../../PiSw/src/McSpectrumTPXFormat.h"
#include "stdio.h"
#include "string.h"
#include <stdio.h>

#pragma warning(disable:4996)

const int MAX_BUFLEN = 100000;
uint8_t srecBuf[MAX_BUFLEN];
uint8_t trecBuf[MAX_BUFLEN];

extern "C"
{
	void utils_goto(uint32_t addr)
	{
		printf("Goto address %08x\n", addr);
	}

	void utils_store_abs8(uint32_t addr, uint8_t data)
	{
		srecBuf[addr] = data;
	}
	void uart_purge()
	{
	}
}

void onData(uint32_t addr, const uint8_t* pData, uint32_t len)
{
	printf("onData %08x %d\n", addr, len);
}

void onAddr(uint32_t addr)
{
	printf("onAddr %08x\n", addr);
}

int main()
{
	McSpectrumTPXFormat tpxFormat;

	const char* filePath = "../../../TargetSW/ZxSpectrum/Games/HoraceGoesSkiing/Horace Goes Skiing.tzx";
	FILE* fp = fopen(filePath, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (size > 1000000)
		{
			printf("Too big!\n");
			fclose(fp);
			return 1;
		}
		uint8_t* fileData = new uint8_t[size];
		size_t readLen = fread(fileData, 1, size, fp);
		fclose(fp);

		tpxFormat.proc(onData, onAddr, fileData, (int)readLen);
		delete[] fileData;
	}
	else
	{
		printf("File not found %s\n", filePath);
	}
	printf("Done - press ENTER to quit\n");
	getchar();
	return 0;
}