
.org $4000

    ld ix, regsBase

    ; Dashed registers
    ld c, (ix+15)
    ld b, (ix+16)
    ld e, (ix+17)
    ld d, (ix+18)
    ld l, (ix+19)
    ld h, (ix+20)
    exx
    ld sp, regsBase+23
    pop af
    ex af, af'

    ; I and R registers 
    ld a, (ix+10)
    ld i,a
    ld a, (ix+11)
    ld r,a

    ; IX/IY
    ld sp, regsBase+27
    pop ix
    pop iy

    ; Main registers - a gets reloaded
    ld sp, regsBase+6
    pop hl
    pop bc
    pop af
    ld de, (regsBase+13)

    ; Fix up SP and use stack to set PC
    ld sp, (regsBase + 8)
    ld hl, (regsBase + 6)
    push hl
    ld hl, (regsBase + 4)
    ret

.org $4200
regsBase:

