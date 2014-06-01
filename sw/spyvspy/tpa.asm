BDOSVEC_HI:	equ 7

    org 100h

    ld c, 9
    ld de, msg_5
    call 5

    ld c, 9
    ld de, msg_sp
    call 5
    ld h, 0
    ld l, h
    add hl, sp
    call DispHLhex

    ld c, 9
    ld de, msg_bdos_vector
    call 5
    ld hl, (6)
    call DispHLhex


    ld c, 9
    ld de, msg_nowalteringsp
    call 5

    ld sp, $1307

    ld c, 9
    ld de, msg_5
    call 5

    ld c, 9
    ld de, msg
    call 5

    ld a, ($ffff)
    ld c, a
    call OutHex8

    ; this exits ok rst 0
    ret

    jp TestMovingToHiMem

ReturnToDump:

    ld hl, $c000

dumploop:
    ld a, l
    and 0fh
    call z, displayRest

    ld c, (hl)
    call space 
    call OutHex8

    inc l
    jr z, dl_nexth
    jr dumploop
dl_nexth:
    inc h
    jr dumploop
    ret

displayRest:
    call checkkey

	; chars
	call space
	
	ld a, l
	sub 10h
	ld l, a
nextchar:
	ld a, (hl)
	cp 20h
	jr nc, charok
	ld a, '.'
charok:	
	call putchar
	inc l
	ld a, l
	and 0fh
	jr nz, nextchar

nextline:
	call nl
	call DispHLhex
	ret

nl:
	ld a, 0dh
	call putchar
	ld a, 0ah
	call putchar
	ret

checkkey:
	push hl
	jr checkkeyret

	rst 30h
	db 70h
	dw 9ch
	jr z, checkkeyret
	rst 30h
	db 70h
	dw 9fh
	rst 30h
	db 70h
	dw 9fh
checkkeyret:
	pop hl
	ret

space:
	ld a, ' '
	call putchar
	ret

putchar:
	push bc
	push de
	push hl
	ld c, 2
	ld e, a
	call 5
	call checkkey
	pop hl
	pop de
	pop bc
	ret


;Display a 16- or 8-bit number in hex.
DispHLhex:
; Input: HL
   ld  c,h
   call  OutHex8
   ld  c,l
OutHex8:
; Input: C
   ld  a,c
   rra
   rra
   rra
   rra
   call  Conv
   ld  a,c
Conv:
   and  $0F
   add  a,$90
   daa
   adc  a,$40
   daa
   call putchar
   ret


msg:    db $0d,$0a,'TPA program launched SLUTSEL=$'

msg_sp:		db $0d,$0a,'SP=$'
msg_bdos_vector:
			db $0d,$0a,'BDOS VECTOR=$'
msg_d831:	db $0d,$0a,'Test message printed via CALL #D831',$0d,$0a,'$'
msg_5:		db $0d,$0a,'Test message printed via CALL #5',$0d,$0a,'$'
msg_5_standardsp:		db $0d,$0a,'Test message printed via CALL #5 with unchanged SP',$0d,$0a,'$'
msg_nowalteringsp:		db $0d,$0a,'Now changing SP to 1300',$0d,$0a,'$'
msg_HiAddr:
		db 'Moving test code to $'

printmsg:
	push af
	push hl
	ld c, 9
	call 5
	pop hl
	pop af
	ret

TestMovingToHiMem:
	; Relocate far_test far
    ld      sp, 9000h	
    ld      a, (BDOSVEC_HI)
    ld      h, a
    ld      l, 0
    push    hl
    ld      hl, (ResidentPartSize)
    ld      de, (Whatever)
    add     hl, de
    xor     a
    ex      de, hl
    pop     hl
    sbc     hl, de
    ld      l, a
    push    hl

    push de
    ld de, msg_HiAddr
    call printmsg
    call DispHLhex
    pop de		

    pop 	hl
    push	hl

    ex      de, hl
    ld      hl, ResidentPartNearLoc ; rel D600
    ld      bc, (ResidentPartSize)
    ldir                    ; Copy code to higher memory

    pop		hl
    jp 		(hl)

Whatever:			dw 0
ResidentPartSize:	dw 0x1600 ; ResidentPartEnd - ResidentPartSrc

ResidentPartNearLoc:
	org $c000
ResidentPartSrc:
	ld c, 9
	ld de, ResidentEvil
	call 5
	jp ReturnToDump

	ds 20024

ResidentEvil: 	db 'Soy la parte temerosa que vive en la alta memoria huhuhuhuu$'

ResidentPartEnd:

