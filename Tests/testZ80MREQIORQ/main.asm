

org 0

loophere:

	ld a,0x55
	ld i,a
	in a,(0xaa)

	jp loophere
