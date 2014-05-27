                ; Bootstrap code based on contents of SPY.COM network utility

                ; MSX-BIOS headers are part of z80asm package
				include 'msx-bios.asm'

TPA_EntryPoint:	equ 100h

ORIGIN:         equ 08D80h

rbdos_EntryPoint:	equ 0E900h
BDOS:				equ 00005h
BDOS_VECTOR:		equ 00006h


byte_0_F267:	equ 0F267h 		; OPEN
byte_0_F270:	equ 0F270h		; DISKREAD
byte_0_F279:	equ 0F279h		; DISKWRT

SLTSL:			equ 0FFFFh		; slut select


BINSIG:			db 0FEh
BINSTART:		dw ORIGIN
BINEND:			dw _end
BINRUN:			dw ORIGIN

				org ORIGIN
RemoteCodeEntryPoint:
                ld      a, 8Fh
                ld      hl, 4040h
                call    RDSLT
                cp      52h 
                jr      nz, skipintro
                ld      a, 0Ch
                call    CHPUT 			; clear screen
                ld      hl, rmtHelloJpg ; Сетевая файловая станция версия 1.0
                                        ;          (C) "ИНФИД" 1989
                                        ;
                                        ; Ждите
                call    rmtPuts

; ---------------------------------------------------------------------------
                rst     30h             ; 8F:4016 Vector to ENDNET
                db 8Fh
                dw 4016h
; ---------------------------------------------------------------------------

                call    rmtInitAndGetStudentNumber

skipintro:
                di

                ld      a, 0AAh         ; Secondary slot select register
                ld      (SLTSL), a      ; Select expansion slot 2 in all 4 pages
                ld      a, 0FFh
                out     (0A8h), a       ; Primary slot register
                                        ; select basic slot 3 (RAM) for every page

rmtInitialCommandLoop:                  
                call    rmtReadByteFromFIFO_Rx
                cp      1
                jr      z, rmtReceiveStuff
                cp      2
                jp      z, rmtLaunchMSXDOS 
                jr      nz, rmtInitialCommandLoop

rmtReceiveStuff:                        
                di
                call    rmtReadByteFromFIFO_Rx
                ld      h, a
                call    rmtReadByteFromFIFO_Rx
                ld      l, a
                call    rmtReadByteFromFIFO_Rx
                ld      b, a
                call    rmtReadByteFromFIFO_Rx
                ld      c, a

rmtReceiveDataLoop:                     

                call    rmtReadByteFromFIFO_Rx
                ld      (hl), a
                inc     hl
                dec     bc
                ld      a, b
                or      c
                jr      nz, rmtReceiveDataLoop
                
                ld      a, 2Eh ; '.'
                out     (98h), a        ; VRAM data read/write

                jr      rmtInitialCommandLoop
; ---------------------------------------------------------------------------

rmtLaunchMSXDOS:                        

                ld      a, 0C3h ; '├'   ; jmp
                ld      (BDOS), a
                ld      ix, (BDOS_VECTOR) ; get the first instruction of BDOS (which is jmp ...)
                inc     ix              ; skip the instruction code
                ld      hl, rbdos_EntryPoint
                ld      (ix+1), h       ; subsitute MSX-DOS BDOS vector with our own rbdos_EntryPoint
                ld      (ix+0), l
                call    rmtZeroSystemArea
; Mystery patches
                ld      a, 0E1h ; 'с'   ; pop hl - why?
                ld      (byte_0_F267), a ; OPEN
                ld      (byte_0_F270), a ; DISKREAD
                ld      (byte_0_F279), a ; DISKWRT

                ld      hl, $D5D3
                ld      sp, hl

                ; Initialize BDOS:
                ;   Patch BIOS Jump table
                call    rbdos_EntryPoint + 3

                ld      c, 0Dh          ; Disk Reset 
                call    5
                ld      c, 09h          ; Print string
                ld      de, MSG_Loaded  ; newline
                call    5

;
;RemoteOSLoop:                           
;                call    TPA_EntryPoint
;                jr      RemoteOSLoop
                jp TPA_EntryPoint

; ****************************************
; * rmtZeroSystemArea 
; **************************************** 
rmtZeroSystemArea:                      
                ld      hl, 5Ch 
                ld      bc, 0A4h 

zsa_zerolup:                              
                ld      (hl), 0
                inc     hl
                dec     bc
                ld      a, b
                or      c
                ret     z
                jr      zsa_zerolup


; ****************************************
; * rmtPuts 
; **************************************** 
rmtPuts:                                
                ld      a, (hl)
                or      a
                ret     z
                call    CHPUT
                inc     hl
                jr      rmtPuts


; ****************************************
; * rmtInitAndGetStudentNumber 
; **************************************** 
rmtInitAndGetStudentNumber:             
                di
                call    rmtGetStudentNumber
                and     0Fh
                ld      bc, 0
                ld      de, 0
                push    af
                call    rmtInit_YM3802
                pop     af
                jr      nz, iigs_ret
                call    rmtFunc_16_BREAK ; Write E to YM3802 R55 (FIFO-Tx Control)
                call    rmtGetStudentNumber

iigs_ret:                                  
                ret

; ---------------------------------------------------------------------------

rmtInit_YM3802:                         
                push    af
                push    de
                push    bc
                di
                ld      a, 80h ; 'А'
                out     (9), a
                nop
                nop
                nop
                nop
                xor     a
                out     (9), a

loc_0_EDBF:                             
                ld      e, 1
                ld      a, 66h ; 'f'
                call    rmtWrite_E_To_YM3802_Register
                ld      d, 3
                ld      e, 0E8h ; 'ш'
                ld      a, 84h ; 'Д'
                call    rmtWrite_E_To_YM3802_Register
                ld      a, d
                or      80h ; 'А'
                ld      e, a
                ld      a, 85h ; 'Е'
                call    rmtWrite_E_To_YM3802_Register
                ld      e, 80h ; 'А'
                ld      a, 3
                call    rmtWrite_E_To_YM3802_Register
                pop     bc
                pop     de
                pop     af
                call    rmtMoreYM3802Config
                ret

; ****************************************
; * rmtMoreYM3802Config 
; **************************************** 
rmtMoreYM3802Config:                    
                push    de
                push    bc
                ld      e, 84h ; 'Д'
                call    rmtWrite_E_to_YM3802_TCRx

mym_eded:                                 
                ld      a, 54h ; 'T'
                call    rmtRead_from_YM3802_Register
                and     1
                jr      nz, mym_eded
                pop     de
                ld      a, 44h ; 'D'
                call    rmtWrite_E_To_YM3802_Register
                ld      e, 10h
                ld      a, 45h ; 'E'
                call    rmtWrite_E_To_YM3802_Register
                ld      e, 85h ; 'Е'
                call    rmtWrite_E_to_YM3802_TCRx
                ld      a, 21h ; '!'
                ld      (rmtREG35_SAV), a ; MCS Register 35 save area
                and     0FEh ; '■'
                ld      e, a
                call    rmtSet_YM3802_RCR

ymc_ee13:                                 
                ld      a, 34h ; '4'
                call    rmtRead_from_YM3802_Register
                and     1
                jr      nz, ymc_ee13
                pop     de
                ld      a, 24h ; '$'
                call    rmtWrite_E_To_YM3802_Register
                ld      e, 10h
                ld      a, 25h ; '%'
                call    rmtWrite_E_To_YM3802_Register
                ld      a, (rmtREG35_SAV) ; MCS Register 35 save area
                ld      e, a


; ****************************************
; * rmtSet_YM3802_RCR 
; **************************************** 
rmtSet_YM3802_RCR:                      
                ld      a, 35h ; '5'    ; RCR
                jr      rmtWrite_E_To_YM3802_Register


; ****************************************
; * rmtGetStudentNumber 
; **************************************** 
rmtGetStudentNumber:                    
                                        ; rmtInitAndGetStudentNumber+16
                ld      e, 80h ; 'А'
                ld      a, 94h ; 'Ф'
                call    rmtWrite_E_To_YM3802_Register
                ld      a, 96h ; 'Ц'
                call    rmtRead_from_YM3802_Register
                push    af
                and     0Fh
                ld      e, 80h ; 'А'
                jr      nz, gsn_snusnu
                ld      e, 0

gsn_snusnu:                               
                ld      a, 95h ; 'Х'
                call    rmtWrite_E_To_YM3802_Register
                pop     af
                ret


; ****************************************
; * rmtRead_from_YM3802_Register 
; **************************************** 
rmtRead_from_YM3802_Register:           
                                        ; rmtMoreYM3802Config+2F ...
                push    bc
                ld      c, a
                rrca
                rrca
                rrca
                rrca
                out     (9), a
                ld      a, c
                and     7
                add     a, 8
                ld      c, a
                in      a, (c)
                pop     bc
                ret


; ****************************************
; * rmtWrite_E_To_YM3802_Register 
; **************************************** 
rmtWrite_E_To_YM3802_Register:          
                                        ; EDCC ...
                push    bc
                ld      c, a
                rrca
                rrca
                rrca
                rrca
                out     (9), a
                ld      a, c
                and     7
                add     a, 8
                ld      c, a
                out     (c), e
                pop     bc
                ret


; ****************************************
; * rmtWrite_E_to_YM3802_TCRx 
; **************************************** 
rmtWrite_E_to_YM3802_TCRx:              
                                        ; rmtMoreYM3802Config+1F
                ld      a, 55h ; 'U'
                jr      rmtWrite_E_To_YM3802_Register

; ---------------------------------------------------------------------------

rmtFunc_16_BREAK:                       
                ld      e, 8Dh ; 'Н'    ; Write E to YM3802 R55 (FIFO-Tx Control)
                call    rmtWrite_E_to_YM3802_TCR
                ld      e, 1

; ****************************************
; * rmtWrite_E_to_YM3802_TCR 
; **************************************** 
rmtWrite_E_to_YM3802_TCR:               
                ld      a, 55h ; 'U'
                jp      rmtWrite_E_To_YM3802_Register


; ****************************************
; * rmtRecvByte0 
; **************************************** 
rmtRecvByte0:                           
                                        ; ED37 ...
                in      a, (1)
                and     2
                jr      z, rmtRecvByte0
                in      a, (0)
                ret


; ****************************************
; * rmtReadByteFromFIFO_Rx 
; **************************************** 
rmtReadByteFromFIFO_Rx:                 ; DATA XREF: PatchUARTtoFIFO
                ld      a, 3
                out     (9), a

rbff_wait:                                 
                in      a, (0Ch)
                and     83h ; 'Г'
                cp      80h ; 'А'
                jr      nz, rbff_wait
                in      a, (0Eh)
                ret

; ---------------------------------------------------------------------------
rmtREG35_SAV:   db    0                 ; DATA XREF: rmtMoreYM3802Config+24
                                        ; rmtMoreYM3802Config+43
                                        ; MCS Register 35 save area


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
   call CHPUT
   ret

MSG_Loaded:		db 1bh,'e',0dh,0ah,'$'

rmtHelloJpg:
				db 0F3h,0C5h,0D4h,0C5h,0D7h,0C1h,0D1h,020h,0C6h,0C1h,0CAh,0CCh,0CFh,0D7h,0C1h,0D1h,020h,0D3h,0D4h,0C1h,0CEh,0C3h,0C9h,0D1h,020h,0D7h,0C5h,0D2h,0D3h,0C9h,0D1h,020h,031h,02Eh,030h,00Dh,00Ah,020h,020h,020h,020h,020h,020h,020h,020h,020h,028h,043h,029h,020h,022h,0E9h,0EEh,0E6h,0E9h,0E4h,022h,020h,031h,039h,038h,039h,00Dh,00Ah,00Dh,00Ah,0F6h,0C4h,0C9h,0D4h,0C5h,020h,000h

_end:
				end
