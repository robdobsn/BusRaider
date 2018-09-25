// Compile using Z88DK https://github.com/z88dk/z88dk

#include <math.h> 

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

void lissajous(float mult1, float mult2, float phase)
{
	int xScale = 100;
	int yScale = 100;
	int xCentre = X_PIXELS/2;
	int yCentre = Y_PIXELS/2;
	float endPt = 3 * M_PI;
	for (float t = 0; t <= endPt; t += 0.005)
	{
		float x = sin(t * mult1 + phase) * xScale + xCentre;
		float y = cos(t * mult2) * yScale + yCentre;
		plot((int)x,(int)y);
	}
}

main()
{
	// Clear screen
	cls();

	// Plot lissajous figure
	lissajous(7.0,6.0,0.84);

	while(1)
		;

}
