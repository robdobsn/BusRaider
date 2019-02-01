

org 0

	ld hl,0x3c00
	ld de,0x3c01
	ld (hl),0x22
	ld bc,0x4400
	ldir

loophere:
	jp loophere
