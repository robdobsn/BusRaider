// Compile using Z88DK https://github.com/z88dk/z88dk

// For machine Rob's Z80
#define ABS(N) (((N)<0)?(-(N)):(N))

#define SCREEN_BASE 0x4000
#define SCREEN_MEM_LEN 0x4000
#define X_PIXELS 512
#define Y_PIXELS 256
#define PIXELS_PER_BYTE 8
#define BYTES_PER_LINE (X_PIXELS / PIXELS_PER_BYTE)

void cls()
{
	char* pScreenBuf = (char*)SCREEN_BASE;
	for (int i = 0; i < SCREEN_MEM_LEN; i++)
		*pScreenBuf++ = 0;
}

void plot(int x, int y)
{
	y = Y_PIXELS - y;
	char* pScreenBuf = (char*)SCREEN_BASE;
	int addr = (x / PIXELS_PER_BYTE) + y * BYTES_PER_LINE;
	*(pScreenBuf+addr) |= (0x80 >> (x % PIXELS_PER_BYTE));
}

void line(int x1, int y1, int x2, int y2)
{
	int diffX = ABS(x2 - x1);
	int diffY = ABS(y2 - y1);
	if (diffX >= diffY)
	{
		// Swap ends if end > start
		if (x1 > x2)
		{
			int tmp = x1;
			x1 = x2;
			x2 = tmp;
			tmp = y1;
			y1 = y2;
			y2 = tmp;
		}
		// Plot initial point
		plot(x1, y1);
		// Iterate over x points
		int yInc = (y2 >= y1) ? 1 : -1;
		int acc = 0;
		int y = y1;
		for (int i = 0; i < diffX; i++)
		{
			acc += diffY;
			if (acc > diffX)
			{
				y += yInc;
				acc -= diffX;
			}
			plot(x1+i, y);
		}
	}
	else
	{
		// Swap ends if end > start
		if (y1 > y2)
		{
			int tmp = x1;
			x1 = x2;
			x2 = tmp;
			tmp = y1;
			y1 = y2;
			y2 = tmp;
		}
		// Plot initial point
		plot(x1, y1);
		// Iterate over x points
		int xInc = (x2 >= x1) ? 1 : -1;
		int acc = 0;
		int x = x1;
		for (int i = 0; i < diffY; i++)
		{
			acc += diffX;
			if (acc > diffY)
			{
				x += xInc;
				acc -= diffY;
			}
			plot(x, y1+i);
		}
	}
}

main()
{
	// Clear screen
	cls();

	// Draw diagonal lines
	#define HSEP 4
	#define HLINES Y_PIXELS / HSEP
	for (int k = 0; k < HLINES; k++)
		line(0, k * HSEP, X_PIXELS-1, Y_PIXELS - k * HSEP);
	#define VSEP 4
	#define VLINES X_PIXELS / VSEP
	for (int k = 0; k < VLINES; k++)
		line(k * VSEP, 0, X_PIXELS - k * VSEP, Y_PIXELS-1);

	while(1)
		;

}
