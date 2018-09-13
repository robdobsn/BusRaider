

org 0

loopstart:
	ld a,0x21
loophere:
	ld hl,0x3c00
	ld de,0x3c01
	ld (hl),a
	ld bc,0x4400
	ldir
	inc a
	cp a,0x7e
	jp nz,loophere
	jp loopstart
