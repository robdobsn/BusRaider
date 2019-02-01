

org 0

	ld sp,0x0000
	ld hl,0x3c00
	ld de,0x3c01
	ld (hl),0x20
	ld bc,0x3ff
	ldir

	ld hl,0x4000
	ld bc,0x1000
	ld a,0x21

loophere:
	cpi
	jp nz, notCorrect
	jp po, doneLoop
	inc a
	jp loophere

doneLoop:
	ld de, 0x3c00
	ld a, 0x31
	ld (de),a

loopforever:
	jp loopforever

notCorrect:
	dec hl
	ld de, 0x3c00
	ld b, a
	push bc
	ld a, 0x33
	ld (de), a
	inc de
	inc de

	ld a, h
	call dispHexAtDE
	ld a, l
	call dispHexAtDE
	inc de

	ld a, (hl)
	call dispHexAtDE
	inc de
	pop bc
	ld a, b
	call dispHexAtDE
	
	jp loopforever

dispHexAtDE:
	ld b, a
	push bc 
	sra a
	sra a
	sra a
	sra a
	and a, 0x0f
	call getNibble
	ld (de),a
	inc de
	pop bc
	ld a,b
	and a,0x0f
	call getNibble
	ld (de), a
	inc de
	ret

getNibble:
	add a, 0x30
	cp a, 0x3a
	jp c, done
	add a, 0x7
done:
	ret
