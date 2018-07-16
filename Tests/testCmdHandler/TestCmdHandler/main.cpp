
#include "../../../PiSw/src/cmd_handler.h"
#include "stdio.h"
#include "string.h"

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

int main()
{
	cmdHandler_init(0, MAX_BUFLEN, trecBuf, MAX_BUFLEN);

	FILE* fp = fopen("../../../PiSw/pi_bus_raider.srec", "r");
	if (fp)
	{
		char line[400];
		while (fgets(line, sizeof line, fp) != NULL)
		{
			int linlen = strlen(line);
			for (int i = 0; i < linlen; i++)
				cmdHandler_handle_char(line[i]);

			//printf("%s", line);
		}
		fclose(fp);
	}

}