// Compile using Z88DK https://github.com/z88dk/z88dk

//#include <stdio.h>

void plot(int x, int y)
{
	char* pScreenBuf = (char*)0xc000;
	int addr = (x / 8) + y * 64;
	*(pScreenBuf+addr) |= (0x80 >> (x % 8));
}

main()
{
//   printf("Hello World 4 !\n");

	// char* pScreenBuf = (char*)0x3c00;
	// *pScreenBuf++ = 'H';
	// *pScreenBuf++ = 'e';
	// *pScreenBuf++ = 'l';
	// *pScreenBuf++ = 'l';
	// *pScreenBuf++ = 'o';
	// while(1)
	// 	;

	char* pScreenBuf = (char*)0xc000;
	for (int i = 0; i < 0x4000; i++)
		*pScreenBuf++ = 0;

	for (int i = 0; i < 256; i++)
		plot(i, i);

	// pScreenBuf = (char*)0xc001;
	// *pScreenBuf = 0x01;

	while(1)
		;

}
