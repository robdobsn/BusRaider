

org 0

	ld hl,0x4000
	ld de,0x4001
	ld (hl),0x01
	ld bc,0x4000
	ldir

loophere:
	jp loophere
