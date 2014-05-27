; Readline from MSX-DOS
; Some hints taken from http://code.google.com/p/aguaviva-libs/source/browse/msx/disk/disk.mac

Hook_F2AC:			equ 0F2ACh
Hook_F2AF:			equ 0F2AFh
Hook_F252:			equ 0F252h


Var_F238:			equ 0F238h  		; console columnpos at start of lineinput
InsertMode_F239:	equ 0F239h
SecretMessageFlag_F23A:	equ 0F23Ah

LineBuffer_F459:	equ 0F459h
CursorPos_F237:		equ 0F237h
PrinterOn_F23B:		equ 0F23Bh

charvalid_F336:		equ 0F336h
char_F337:			equ 0F337h

CtrlC_Handler_F325:	equ 0F325h

EvilReturn_F1E8:	equ 0F1E8h


; ---------------------------------------------------------------------------
; Programmers message, decoded in FAT entries (not needed)
; MSXDOS BY T PATERSON J SUZUKI @
; To show: enter 127 chars (full line and BS), Ctrl+J and BS
SecretMessage: 	
		db 0EDH,08CH,093H,0EFH,00CH,088H,039H,040H
		db 003H,070H,048H,097H,0F2H,0FCH,0BAH,080H
		db 00AH,0CCH,0B5H,05EH,0AFH,029H,000H,000H


DecodeSecretCharacter:  
                call    _piece_of_fat
                ld      a, l
                add     hl, hl
                add     hl, hl
                and     3Fh 
                ret

_piece_of_fat: 
                call    Hook_F252
                push    de
                ld      e, l
                ld      d, h
                srl     h
                rr      l
                rra
                add     hl, de
                pop     de
                add     hl, de
                rla
                ld      a, (hl)
                inc     hl
                ld      h, (hl)
                jr      nc, _this_is_fat
                srl     h
                rra
                srl     h
                rra
                srl     h
                rra
                srl     h
                rra
_this_is_fat:     
                ld      l, a
                ld      a, h
                and     0Fh
                ld      h, a
                or      l
                ret
; ---------------------------------------------------------------------------



PrintOnPrinter:
				ret

Echo:                                   
                push    ix
                rst 30h
                db 70h
                dw 0A2h
                pop     ix
                ret



;CheckSTOP:
;              
;                push    ix
;                rst 30h 				
;                db 70h
;                dw 0B7h
;                pop     ix
;                jr      nc, _checkstop_1
;                ld      a, 3
;                ld      (charvalid_F336), a
;                ld      (char_F337), a
;                and     a
;                ret
;; ---------------------------------------------------------------------------
;_checkstop_1:                             
;                ld      a, (charvalid_F336)
;                and     a
;                ld      a, (char_F337)
;                ret     nz
;                push    ix
;                rst     30h             ; CHSNS Tests the status of the keyboard buffer
;                db 70h
;                dw 9Ch
;                pop     ix
;                ret     z
;                ld      a, 0FFh
;                ld      (charvalid_F336), a
;                push    ix
;                rst     30h             ; CHSNS Tests the status of the keyboard buffer
;                db 70h
;                dw 9Fh
;                pop     ix
;                ld      (char_F337), a
;                push    bc
;                ld      b, 0
;                inc     b
;                pop     bc
;                ret

; In most of the places	inside:
; B = 0-based buffer index
; C = buffer size (like	0x80)
;
; Input:
;   DE points to a buffer
;   DE[0] == buffer length
;   DE[1] == actual line length
;

; Func 0A is called with DE=D34E
; but here we should be getting with DE=DA40
; DA3E stores the original DE
; DA40 is the secret internal buffer 

; This entry point normally resides at 50E0 in diskrom
BufferedInputEntry:
		push	de
		ld	a, (CursorPos_F237)
		ld	(Var_F238), a
		xor	a 								
		ld	(InsertMode_F239), a 			; reset Ins mode
		ld	h, d
		ld	l, e 							; HL = DE = buffer ptr
		ld	b, a
		ld	c, (hl) 						; C = buffer length
		inc	hl 
		ld	d, a 					
		ld	e, (hl)
		inc	hl
		ld	ix, LineBuffer_F459
		ld	a, e
		cp	c
		jr	nc, _BufferedInputEntry_L1
		push	hl
		add	hl, de
		ld	a, (hl)
		pop	hl
		cp	0Dh
		jr	z, BufferedInputEntry_5104

_BufferedInputEntry_L1:
		ld	e, d

BufferedInputEntry_5104:
		call	Hook_F2AC
		call	getchar_handling_printer_hotkeys

DispatchCharHandler:			
		push	hl
		push	bc
		ld	hl, SeventeenSpecialChars ; find if A is among the chars in SpecialChars
		ld	bc, 11h
		cpir
		add	hl, bc
		add	hl, bc
		add	hl, bc
		ld	c, (hl)
		inc	hl
		ld	h, (hl)
		ld	l, c
		pop	bc
		ex	(sp), hl
		ret


; ---------------------------------------------------------------------------

_511E_Handle_01_CtrlA:	
		call	getchar_handling_printer_hotkeys
		cp	40h ; '@'
		jr	c, DispatchCharHandler
		cp	60h ; '`'
		jr	nc, DispatchCharHandler
		push	af
		ld	a, b					; Check	if we're at the limit of the buffer
		inc	a
		cp	c
		jr	nc, CharDone
		ld	a, 1
		ld	(ix+0),	a
		inc	ix
		inc	b
		call	EchoControlCharacter ; Print control character like ^Q
		pop	af

NormalChar_513B:
		push	af
		ld	a, b					; B = 0-based current char index, C = max length
		cp	c
		jr	nc, CharDone			; Check	if we're at the limit of the buffer
		pop	af
		ld	(ix+0),	a
		inc	ix
		inc	b
		call EchoControlCharacter 	; Print control character like ^Q
		ld	a, (InsertMode_F239)
		or	a
		jr	nz, BufferedInputEntry_5104
		inc	d
		ld	a, e
		cp	d
		jr	c, BufferedInputEntry_5104
		ld	a, (hl)
		dec	a
		inc	hl
		jr	nz, BufferedInputEntry_5104
		inc	d
		inc	hl
		jr	BufferedInputEntry_5104
; ---------------------------------------------------------------------------

CharDone:
		pop	af
		ld	a, 7		; bell
		call	EchoNormalCharacter
		jr	BufferedInputEntry_5104
; ---------------------------------------------------------------------------

		; Clear	current	line
_5166_Handle_15_ClearLine:		
		pop	de		
		ld	a, (Var_F238)
		ld	b, a
		ld	a, (CursorPos_F237)
		sub	b
		jr	z, _nothingtorubout
		ld	b, a

_ruboutloop:
		call	Rubout
		djnz	_ruboutloop

_nothingtorubout:
		jp	BufferedInputEntry 

; ---------------------------------------------------------------------------

_517A_Handle_CtrlJ:	
		ld	a, b
		ld	(SecretMessageFlag_F23A), a
		call	NewLine

_keyhandler_exit:	
		jr	BufferedInputEntry_5104


NewLine:
		ld	a, 0Dh
		call	EchoNormalCharacter
		ld	a, 0Ah
		jp	EchoNormalCharacter

_518D_Handle_CtrlM:
		pop	de

KillLine:				
		call	EchoNormalCharacter
		push	de
		inc	de
		ld	a, b
		ld	(de), a
		cp	c
		push	af
		inc	de
		ld	c, b
		xor	a
		ld	b, a
		or	c
		jr	z, _kl_nothingtoclear
		ld	hl, LineBuffer_F459
		ldir

_kl_nothingtoclear:			
		pop	af
		jr	z, _kl_nocr
		ld	a, 0Dh
		ld	(de), a

_kl_nocr:				
		pop	de
		ret

		;
		; Bullshit handler
		;
_51AB_Handle_BS:
		ld	a, (SecretMessageFlag_F23A)
		and	b
		cp	7Fh 
		jp	z, DisplaySecretMessage

		inc	b
		dec	b
		jr	z, _BS_Exit 				; offset == 0 -> 51DC

		dec	b 							; dec string offset
		dec	ix	; dec input pointer
		call Rubout 					; wipe last char
		inc	b
		dec	b 							; are we at 0?
		jr	z, _BS_Cont 				; yes ->
		dec	b 							; look back one more char?
		dec	ix 							; ptr--
		ld	a, (ix+0) 	 	  			
		dec	a
		jr	z, _BS_Exit 				; if *ptr == 1 it's totally ok, go nax
		inc	b 							; restore b 
		inc	ix 							; restore ix

_BS_Cont:
		ld	a, (ix+0) 	
		cp	20h 
		jr	nc, _BS_Exit 				; >= 20h, no special action, exit
		cp	9 
		jr	z, _BS_RuboutTab 			; Tab? Rubout tab
		call	Rubout 					; ^Q and the like, rubout second char
										; then leave
		; Intricate BS leave routine that seems to be do some cleanup work
_BS_Exit:
		ld	a, (InsertMode_F239)
		or	a
		jr	nz, _keyhandler_exit 	; if Insert, exit
		inc	d
		dec	d
		jr	z, _keyhandler_exit 	; if that other offs == 0, exit
		dec	d
		ld	a, d
		cp	e
		jr	nc, _keyhandler_exit 	; if that other ofs < other length, exit
		dec	hl 
		ld	a, d
		cp	1
		jr	c, _keyhandler_exit
		dec	hl
		ld	a, (hl)
		dec	a
		inc	hl
		jr	nz, _keyhandler_exit
		dec	d
		dec	hl
		jr	_keyhandler_exit

_BS_RuboutTab:
		push	hl
		push	bc
		ld	a, (Var_F238)
		ld	c, a
		inc	b
		dec	b
		jr	z, _BS_521A
		ld	hl, LineBuffer_F459

_BS_loop_5208:
		ld	a, (hl)
		inc	hl
		cp	1
		jr	z, _BS_5218
		inc	c
		cp	20h ; ' '
		jr	nc, _BS_5218
		cp	9
		jr	z, _BS_522A
		inc	c

_BS_5218:
		djnz	_BS_loop_5208

_BS_521A:
		ld	a, (CursorPos_F237)
		sub	c
		jr	z, _BS_5226
		ld	b, a

_BS_loop_5221:
		call	Rubout
		djnz	_BS_loop_5221

_BS_5226:
		pop	bc
		pop	hl
		jr	_BS_Exit
; --[ End of bullshit handler ]------------------------------------------

_BS_522A:
		ld	a, c
		add	a, 7
		and	0F8h ; 'ø'
		ld	c, a
		jr	_BS_5218

		; Toggle Insert mode
_5232_Handle_12_Insert:	
		ld	a, (InsertMode_F239)
		xor	1
		ld	(InsertMode_F239), a
		jp	BufferedInputEntry_5104

DisplaySecretMessage:
		xor	a
		ld	(SecretMessageFlag_F23A), a
		push	bc
		ld	b, 10h
		ld	de, SecretMessage
		ld	hl, 0

dsm_loop:	
		push	hl
		call	DecodeSecretCharacter
		add	a, 20h ; ' '
		call	EchoNormalCharacter
		ld	a, h
		add	a, 20h ; ' '
		call	EchoNormalCharacter
		pop	hl
		inc	hl
		djnz	dsm_loop
		pop	bc

		; Kill input line
_5265_Handle_CtlrK:
		ld	a, '@'    
		pop	de
		call	KillLine
		call	NewLine
		ld	a, (Var_F238)
		or	a
		jp	z, BufferedInputEntry
		ld	b, a
		ld	a, 20h ; ' '

_wipeloop:
		call	EchoNormalCharacter
		djnz	_wipeloop
		jp	BufferedInputEntry

		; Arrow down, recall entire buffer
_5280_Handle_1F_Recall:		
		ld	a, 0FFh		
		jr	RecallLine

		; Ctrl-L, CLS - ? 
_5284_Handle_CLS:
		call Unexplainable
		jp	c, BufferedInputEntry_5104
		push	bc
		ld	c, a
		ld	b, 0
		add	hl, bc
		pop	bc
		add	a, d
		ld	d, a
		jp	BufferedInputEntry_5104

		; SELECT key, COPYUP ?
_5295_Handle_18_CtrlX:
		call Unexplainable
		jp	c, BufferedInputEntry_5104
		jr	RecallLine

_529D_Handle_Skip:				
		ld	a, d
		cp	e
		jp	nc, BufferedInputEntry_5104
		inc	d
		ld	a, (hl)
		dec	a
		inc	hl
		jp	nz, BufferedInputEntry_5104
		inc	d
		inc	hl
		jp	BufferedInputEntry_5104

		; Recall old line char by char
_52AE_Handle_1C_CursorRight:
		ld	a, (hl)
		dec	a
		ld	a, 1
		jr	nz, RecallLine
		inc	a

		; A = maximum line length
		; IX points to current editor buffer (LineBuffer_F459)
		; HL points to previous buffer (like DA42)
RecallLine:
		push	af
		xor	a					; the holy rule	of microsoft: never let	people insert
		ld	(InsertMode_F239), a ; use every chance	to clear Insert	mode
		ld	a, b				; current char offset
		cp	c					; compare to max
		jr	nc, _rl_Done		; leave if >= buffer length
		ld	a, d				; E = length of	stored buffer
		cp	e					; D seems to be	offset,	equal B
		jr	nc, _rl_Done		; leave if not within the limits

		ld	a, (hl)				; a = *hl, old char

		cp	1
		jr	nz, _rl_normal 		; \001 has holy purpose
		ld	a, b
		inc	a
		cp	c
		jr	nc, _rl_Done
		ld	a, (hl)

_rl_normal:	
		inc	hl 					; increment source ptr
		ld	(ix+0),	a 			; store character
		inc	ix  				; increment destination ptr
		call EchoControlCharacter 
		inc	b 					; increment dst offset
		inc	d 					; increment src offset
		pop	af 					; check length counter
		dec	a
		jr	nz, RecallLine	
		jp	BufferedInputEntry_5104

_rl_Done:
		pop	af
		jp	BufferedInputEntry_5104

; ---------------------------------------------------------------------------


; I cannot explain this
Unexplainable:
		call	getchar_handling_printer_hotkeys
		cp	1
		jr	nz, unexplainable_sub_531F
		call	getchar_handling_printer_hotkeys
		cp	40h ; '@'
		jr	c, unexplainable_sub_531F
		cp	60h ; '`'
		jr	nc, unexplainable_sub_531F
		push	hl
		push	de
		push	bc
		ld	iy, 0

_unexplainable_loop_52FC:
		scf
		push	af
		ld	a, 1
		call	unexplainable_sub_531F
		jr	c, _unexplainable_exit_531A
		ld	c, a
		ld	b, 0
		add	hl, bc
		add	a, d
		ld	d, a
		push	iy
		pop	af
		add	a, c
		push	af
		pop	iy
		inc	hl
		pop	af
		cp	(hl)
		dec	hl
		jr	nz, _unexplainable_loop_52FC
		push	iy

_unexplainable_exit_531A:		
		pop	af
		pop	bc
		pop	de
		pop	hl
		ret

unexplainable_sub_531F:		
		push	bc
		push	af
		ld	a, e
		sub	d
		jr	c, _ret_with_CY
		jr	z, _ret_with_CY
		dec	a
		jr	z, _ret_with_CY
		ld	c, a
		ld	b, 0
		pop	af
		push	hl
		push	af
		ld	a, (hl)
		dec	a
		jr	nz, us_loc_5336
		inc	hl
		dec	c

us_loc_5336:				
		pop	af
		inc	c
		dec	c
		jr	nz, us_loc_533F
		pop	hl
		pop	bc
		scf
		ret

us_loc_533F:				
		inc	hl
		cpir
		pop	hl
		jr	nz, _usloc_534C
		ld	a, e
		sub	d
		dec	a
		sub	c
		pop	bc
		ret
_ret_with_CY:				
		pop	af

_usloc_534C:				
		pop	bc
		scf
		ret


		; Go back one position, print space, go back again
Rubout:					
		ld	a, 8
		call	EchoNormalCharacter
		ld	a, 20h ; ' '
		call	EchoNormalCharacter
		ld	a, 8
		jr	EchoNormalCharacter

		; Print	control	character like ^Q
EchoControlCharacter:			
		cp	20h ; ' '
		jr	nc, EchoNormalCharacter
		cp	9
		jr	z, EchoNormalCharacter
		cp	1
		jr	z, EchoNormalCharacter
		push	af
		ld	a, '^'
		call	EchoNormalCharacter
		pop	af
		or	40h ; '@'
		jr	EchoNormalCharacter


; ---------------------------------------------------------------------------
SeventeenSpecialChars:db 6, 7Fh, 8, 0Dh, 0Ah, 15h, 0Bh,	0Ch, 1Bh, 12h, 18h, 1Ch, 1Dh, 1Eh, 1Fh,	1, 8
					
Vector_NormalChar:dw NormalChar_513B
		dw _511E_Handle_01_CtrlA ; 1, Ctrl+A
		dw _5280_Handle_1F_Recall ; 1F
		dw _5166_Handle_15_ClearLine ; 15, Ctrl+U
		dw _51AB_Handle_BS	; 8, BS
		dw _52AE_Handle_1C_CursorRight	; 1C, Ctrl+\
		dw _5295_Handle_18_CtrlX ; 18, Ctrl+X
		dw _5232_Handle_12_Insert ; 12,	Ctrl+R
		dw _5166_Handle_15_ClearLine ; 1B, ESC
		dw _5284_Handle_CLS	; 0C, Ctrl+L
		dw _5265_Handle_CtlrK	; 0B, Ctrl+K - Kill
		dw _5166_Handle_15_ClearLine ; 1E Cursor up
		dw _517A_Handle_CtrlJ	; 0A, Ctrl+J, NL
		dw _518D_Handle_CtrlM	; 0D, Ctrl+M, CR
		dw _51AB_Handle_BS	; 1D
		dw _529D_Handle_Skip		;  ?
		dw BufferedInputEntry_5104

BDOS_FUNC_X:
		ld	a, e

EchoNormalCharacter:			
		call Hook_F2AF
		cp	0Bh		; Ctrl+K - kill	line
		jr	z, Echo_EndLine
		cp	0Ch		; Ctrl+L
		jr	z, Echo_EndLine
		cp	1Ch		; Ctrl+\
		jr	z, CheckStopAndXoffThenEcho
		cp	1Dh		; Ctrl+]
		jr	z, Echo_Bullshit
		cp	0Dh		; CR
		jr	z, Echo_EndLine
		cp	8		; Echo_Bullshit
		jr	z, Echo_Bullshit
		cp	9		; TAB
		jr	z, Echo_TabKey
		cp	7Fh ; ''
		jr	z, Echo_Bullshit
		cp	20h ; ' '
		jr	c, CheckStopAndXoffThenEcho
		push	hl
		ld	hl, CursorPos_F237
		inc	(hl)
		pop	hl

CheckStopAndXoffThenEcho:		
		push	bc
		ld	b, a
		call	CheckSTOPandXOFF
		ld	a, b
		call	Echo
		ld	a, (PrinterOn_F23B)
		or	a
		ld	a, b
		pop	bc
		ret	z
		jp	PrintOnPrinter

; ---------------------------------------------------------------------------

Echo_EndLine:				
		push	af
		xor	a
		ld	(CursorPos_F237), a
		pop	af
		jr	CheckStopAndXoffThenEcho

		; Echo on BS press
Echo_Bullshit:				
		push hl
		ld	hl, CursorPos_F237
		dec	(hl)
		pop	hl
		jr	CheckStopAndXoffThenEcho

		; Fill to the border of the next tab
Echo_TabKey:					
		ld	a, 20h ; ' '
		call	EchoNormalCharacter
		ld	a, (CursorPos_F237)
		and	7
		jr	nz, Echo_TabKey
		ret

NoXoff:					
		cp	10h
		jr	z, getchar_handling_ctrl_c
		cp	0Eh
		jr	z, getchar_handling_ctrl_c
		cp	3
		jr	z, getchar_handling_ctrl_c
		ret

CheckSTOPandXOFF:			
		call	CONST_unsafe
		ret	z
		cp	13h		; XOFF/Ctrl+S
		jr	nz, NoXoff
		call	CONIN_unsafe

getchar_handling_ctrl_c:		
		call	CONIN_unsafe
		cp	10h		; Ctrl+P - printer on
		jr	z, PrinterOnSet
		cp	0Eh		; Ctrl+N - printer off
		jr	z, PrinterOnReset
		cp	3		; STOP
		ret	nz
		ld	hl, (CtrlC_Handler_F325)
		jp	EvilReturn_F1E8	; Call to HL in	ram, return with BDosRom on

PrinterOnSet:				
		ld	a, 1
		ld	(PrinterOn_F23B), a
		ret

PrinterOnReset:				
		xor	a
		ld	(PrinterOn_F23B), a
		ret


getchar_handling_printer_hotkeys:	
					; RAM:_511E_Handle_01_CtrlAp ...
		call	getchar_handling_ctrl_c
		jr	z, getchar_handling_printer_hotkeys
		ret


