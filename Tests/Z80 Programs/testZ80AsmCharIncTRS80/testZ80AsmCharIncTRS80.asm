

org 0

loopstart:
	ld e,0x30
loophere:
	ld hl,0x3c00
	ld (hl),e
	ld bc,0x0001
loopin:
	dec bc
	ld a,b
	or c
	jp nz, loopin
	inc e
	ld a, e
	cp a,0x3a
	jp nz,loophere
	jp loopstart
