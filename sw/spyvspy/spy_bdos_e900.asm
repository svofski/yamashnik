                ; Resident part of network MSX-DOS (Net-BDOS)
                ; Based on parts of SPY.COM network utility

ColdStart:	equ 0
CurrentDMAAddr: equ $F23D
CAPST:		equ $FCAB
GETPNT:		equ $F3FA
PUTPNT:		equ $F3F8
STORESP:	equ $F304
SLTSL:		equ 0FFFFh		; secondary slot select

SIZEOF_FCB:     equ $2c
DELAY_SENDWORD: equ 400h 

DEBUG:		        equ 0
DEBUG_BDOSFUNC:         equ 0 ; 1==1
DEBUG_STACK:            equ 0

		org $E900

EntryPoint:     jp EntryPoint_
		jp Init
EntryPoint_:               
                ld	(STORESP), sp
                ld 	sp, $DC00 
		push ix
                push iy

                ld      (SaveDE), de
;--
		jr nodebug 

                ld      (SaveHL), hl
                ld      (SaveBC), bc

if DEBUG_STACK
                ld a, '['
                call DispCharInA
		ld hl, (STORESP)
		ld b, 4
stackloop:
                push bc
		ld e, (hl)
		inc hl
		ld d, (hl)
		inc hl
		push hl
		ex de, hl
		dec hl 
		dec hl
		dec hl
		call DispHLhex
		call DispSpace
		pop hl
		pop bc
		djnz stackloop
                ld a, ']'
                call DispCharInA
endif
if DEBUG_BDOSFUNC
                ld a, '<'
                call DispCharInA
                ld bc, (SaveBC) 
                call OutHex8            ; print function no
                ld a, '>'
                call DispCharInA
                ld a, 0dh
                call CONOUT_unsafe
                ld a, 0ah
                call CONOUT_unsafe
endif
                ld hl, (SaveHL)
                ld de, (SaveDE)
                ld bc, (SaveBC)                
nodebug:
;--

                ; put local return address on stack
                ld	de, ExitPoint
                push 	de
                ld	de, (SaveDE)

                push    bc
                ld      b, a
                ld      a, 0Dh
                cp      c               ; 0d - c
                ld      a, b
                pop     bc
                jp      nc, CustomDispatch ; Dispatch BDOS function in C to custom hooks from DispatchTable
                push    bc
                ld      b, a
                ld      a, 31h ; '1'    ; function less than GET DISK PARAMETERS (31H)
                cp      c
                pop     bc
                ret     c               ; unsupported func, return with C
                push    af
                ld      a, c
                cp      1Ah             ; Set disk transfer address
                jr      z, NoDiskDispatch
                cp      2Ah ; '*'       ; Get date
                jr      z, NoDiskDispatch
                cp      2Bh ; '+'       ; Set date
                jr      z, NoDiskDispatch
                cp      2Ch ; ','       ; Get time
                jr      z, NoDiskDispatch
                cp      2Dh ; '-'       ; Set time
                jr      z, NoDiskDispatch
                cp      2Eh ; '.'       ; Set/reset verify flag
                jr      z, NoDiskDispatch
                pop     af
                jr      DiskIOFunc

; ---------------------------------------------------------------------------
ExitPoint:
		pop iy
		pop ix
		ld 		sp,(STORESP)
		ret
; ---------------------------------------------------------------------------


NoDiskDispatch:                   
                                        
                pop     af
                jp      CustomDispatch ; Dispatch BDOS function in C to custom hooks from DispatchTable
; ---------------------------------------------------------------------------

DiskIOFunc:              
                call    ToggleCAPS
                push    af
                push    de
                push    hl
                push    bc
                ld      a, (ADR1_SendersNumber)
                or      a
                jr      nz, SendersAddressKnown
                ld      a, 8Fh ; 'П'
                ld      hl, 7C9Dh
; ---------------------------------------------------------------------------
                rst     30h             ; RDSLT
                                        ;   Slot =0x8F, NetBIOS
                                        ;   Addr = 0x7C9D: ADR1: Sender's number
                                        ; Return A = Value
                db 70h
                dw 0Ch
; ---------------------------------------------------------------------------
                ld      (ADR1_SendersNumber), a

SendersAddressKnown:              
                call    WaitUntilByteReceived

WaitUntilCalledByServer:          
                ld      a, 7
; ---------------------------------------------------------------------------
                rst     30h             ; SNSMAT Returns the value of the specified line from the keyboard matrix
                                        ; Input    : A  - for the specified line
                                        ; Output   : A  - for data (the bit corresponding to the pressed key will be 0)
                db 70h
                dw 141h
; ---------------------------------------------------------------------------
                bit     2, a
                jr      z, ClearKeyboardBuffer
                call    FIFO_ReceiveByteImmediate
                or      a
                jr      z, WaitUntilCalledByServer ; Nothing received, wait more
                ld      hl, ADR1_SendersNumber
                cp      (hl)
                jr      nz, WaitUntilCalledByServer ; message not for us, wait more
                call    FIFO_ReceiveByteWait
                cp      0F4h ; 'Ї'      ; PING by server
                jr      nz, WaitUntilCalledByServer
                ld      d, 0FFh         ; Reply PONG
                call    FIFO_SendByte
                di
                call    WaitUntilByteReceived
                pop     bc
                push    bc              ; c = function number
                ld      a, c
                sub     0Eh
                ld      d, a
                call    FIFO_SendByte ; send to server BDOS function number - 0x0E
                pop     bc
                pop     hl
                pop     de
                pop     af
                ;call    ToggleCAPS
                call    CustomDispatch ; Dispatch BDOS function in C to custom hooks from DispatchTable
                call    RestoreCAPS
                ei
                ret
; ---------------------------------------------------------------------------

ClearKeyboardBuffer:              
                pop     bc
                pop     hl
                pop     de
                pop     af
                rst     30h             ; CHGET (Waiting)
                db      70h
                dw      9Fh
                ld      hl, (PUTPNT)    ; points to adress to write in the key buffer
                ld      (GETPNT), hl    ; points to adress to write in the key buffer
                ld      a, 0FFh
                ret

; ****************************************
; * ToggleCAPS 
; **************************************** 
; Invert CAPS LED status 
ToggleCAPS:                  
                ex      af, af'
                ld      a, (CAPST)      ; a = CAPS status (00 = Off / FF = On)
                or      a
                jr      nz, caps_light_off
                jr      caps_light_on

; ****************************************
; * RestoreCAPS 
; **************************************** 
; Restore original CAPS LED status
RestoreCAPS:                       
                ex      af, af'
                ld      a, (CAPST)
                or      a
                jr      z, caps_light_off

                ; PPI command register AB
                ; Bit 0:        value to set
                ;     1-3:      bit number within AA
                ; PPI register C: AA bit 6: keyboard CAPS LED, 1 = off
                ; BIOS Variable CAPST: 00 = off/FF = on
caps_light_on:                     
                ld      (CAPST), a
                ld      a, 0Ch          ; AB = 0000 110 0: write 0 to PPI C.6
                out     (0ABh), a       ; turn CAPS light on
                ex      af, af'
                ret
caps_light_off:
                ld      (CAPST), a
                ld      a, 0Dh          ; AB = 0000 110 1: write 1 to PPI C.6
                out     (0ABh), a       ; turn CAPS light off
                ex      af, af'
                ret
;
; Dispatch BDOS function in C to custom hooks from DispatchTable
;
CustomDispatch:                   
                ei
                push    af
                push    bc

                ld      a, c
                add     a, a
                ld      ix, DispatchTable
                ld      b, 0
                ld      c, a
                add     ix, bc
                ld      b, (ix+1)
                ld      c, (ix+0)
                push    bc
                pop     ix
                pop     bc
                pop     af

                jp      (ix)

; ---------------------------------------------------------------------------

Func0_ProgramTerminate:           
                jp      ColdStart
; ---------------------------------------------------------------------------

Func1_ConsoleInput:               
                rst     30h             ; CHSNS Tests the status of the keyboard buffer
                db 70h
                dw 9Ch

                jr      z, Func1_ConsoleInput ; CHSNS Tests the status of the keyboard buffer
                call    Func7_ConsoleInput
                cp      3               ; Ctrl+C
                jr      z, Func0_ProgramTerminate
                ld      e, a

Func2_ConsoleOutput:              
                ;rst 	30h
				;db 70h
				;dw 9ch

                ;jr      z, CHGET_waiting_bufferfull
                ;call    Func7_ConsoleInput
                ;cp      3
                ;jr      z, Func0_ProgramTerminate

CHGET_waiting_bufferfull: 
                ld      c, e
                jp CONOUT_unsafe
; ---------------------------------------------------------------------------

Func3_AUXin:                      
                ret                     ; AUX in
; ---------------------------------------------------------------------------

Func4_AUXout:                     
                ld      a, e
                rst     30h             ; Output to current output channel (printer, diskfile, etc.)
                db 70h
                dw 18h
                ret
; ---------------------------------------------------------------------------

Func5_PrinterOutput:              
                ld      a, e
                rst     30h
                db 70h
                dw 0A5h	
                ret

Func6_ConsoleIO:     
                ld      a, 0FFh
                cp      e
                ld      a, e
                jr      nz, Func2_ConsoleOutput ; CHSNS Tests the status of the keyboard buffer
                                        	; Z-flag set if buffer is filled
		call CONST_unsafe
		ret z
		jp CONIN_unsafe

; ****************************************
; * Func7_ConsoleInput 
; **************************************** 
Func7_ConsoleInput:               
                jp CONIN_unsafe
; End of function Func7_ConsoleInput

; ---------------------------------------------------------------------------

Func8_ConsoleInputNoEcho:         
Func8_ConsoleInputNoEcho_expect:
                rst     30h             ; CHSNS Tests the status of the keyboard buffer
                db      70h
                dw      9Ch
                jr z, Func8_ConsoleInputNoEcho_expect
                jp CONIN_unsafe

; ---------------------------------------------------------------------------

Func9_StringOutput:                                                       
                ld      a, (de)
                cp      24h ; '$'
                ret     z
                push    de
                ld      e, a
                call    Func2_ConsoleOutput 
                pop     de
                inc     de
                jr      Func9_StringOutput
; ---------------------------------------------------------------------------

FuncA_BufferedLineInput:          
				jp BufferedInputEntry
                ;ret

; CHSNS Tests the status of the keyboard buffer
FuncB_ConsoleStatus: 
                rst     30h
                db      70h
                dw      9Ch
                jr      z, FuncB_ConsoleStatus_clra
                ld      a, 0FFh
                ret
; ---------------------------------------------------------------------------

FuncB_ConsoleStatus_clra:                              
                xor     a
                ret
; End of function FuncB_ConsoleStatus

; ---------------------------------------------------------------------------

FuncC_VersionNumber:              
                ld      hl, 22h ; '"'
                ret
; ---------------------------------------------------------------------------

FuncD_DiskReset:                  
                ld      de, 80h ; 'А'   ; Set DMA to 0x0080
                jr      Func1A_SetDMA
; ---------------------------------------------------------------------------

FuncE_SelectDisk:                 
                ld      d, e
                jp      FIFO_SendByte
; ---------------------------------------------------------------------------

FuncF_OpenFile:                   
                call    SendFCB
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func10_CloseFile:                 
                call    SendFCB
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func11_SearchFirst:               
                                        
                call    SendFCB
                call    ReceiveChunkToDMA
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func12_SearchNext:                
                jr      Func11_SearchFirst
; ---------------------------------------------------------------------------

Func13_DeleteFile:                
                call    SendFCB
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func14_SequentialRead:            
                call    SendFCB
                call    ReceiveChunkToDMA
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func15_SequentialWrite:           
                push    bc
                ld      bc, 1388h
                call    DelayBC
                pop     bc
                call    SendFCB
                push    bc
                ld      bc, 1388h
                call    DelayBC
                pop     bc
                call    Send128BytesFromDMA
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func16_CreateFile:                
                call    SendFCB
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func17_RenameFile:                
                ld      h, d
                ld      l, e
                ld      bc, SIZEOF_FCB
                call    bdos_SendDataChunk ; Send data chunk &HL, BC = Length
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func18_GetLoginVector:            
                call    FIFO_ReceiveByteWait
                ld      h, a
                call    FIFO_ReceiveByteWait
                ld      l, a
                ret
; ---------------------------------------------------------------------------

Func19_GetCurrentDrive:           
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func1A_SetDMA:                    
                ld      (CurrentDMAAddr), de
                ret
; ---------------------------------------------------------------------------

Func1B_GetAllocInfo:
                ld      d, e
                call    FIFO_SendByte
                call    FIFO_ReceiveByteWait    ; address to receive next chunk (DPB): 0xF195
                ld      h, a
                call    FIFO_ReceiveByteWait
                ld      l, a
                push    hl
                pop     ix                      ; IX = pointer to DPB
                call    ReceiveDataChunk        ; DPB<0xf3>

                call    FIFO_ReceiveByteWait    ; address to receive next chunk (FAT): 0xE595
                ld      h, a
                call    FIFO_ReceiveByteWait
                ld      l, a
                push    hl
                pop     iy                      ; IY = pointer to the first sector of FAT
                call    ReceiveDataChunk        ; receive first sector of FAT
                call    FIFO_ReceiveByteWait    ; BC = word secor size (always 512)
                ld      b, a
                call    FIFO_ReceiveByteWait
                ld      c, a
                call    FIFO_ReceiveByteWait    ; DE = total clusters
                ld      d, a
                call    FIFO_ReceiveByteWait
                ld      e, a
                call    FIFO_ReceiveByteWait    ; HL = free clusters
                ld      h, a
                call    FIFO_ReceiveByteWait
                ld      l, a
                jp      FIFO_ReceiveByteWait    ; sectors per cluster
; ---------------------------------------------------------------------------

Func21_RandomRead:          
		call 	SendDebugBlock      
                call    SendFCB
                call    ReceiveChunkToDMA
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func22_RandomWrite:               
                call    SendFCB
                call    Send128BytesFromDMA
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func23_GetFileSize:               
                call    SendFCB
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func24_SetRandomRecord:           
                call    SendFCB
                jp      ReceiveDataToArg
; ---------------------------------------------------------------------------

Func26_RandomBlockWrite:          
;                push    bc
;                ld      bc, 3E8h
;                call    DelayBC
;                pop     bc
;                ld      d, h
;                call    FIFO_SendByte
;                ld      d, l
;                call    FIFO_SendByte
;                push    bc
;                ld      bc, 2710h
;                call    DelayBC
;                pop     bc
;                ld      ix, (SaveDE)
;                ld      a, (ix+0Eh)
;                ex      de, hl
;                call    Mul_DE_by_A
;                ld      b, h
;                ld      c, l
;                ld      hl, (CurrentDMAAddr)
;                call    bdos_SendDataChunk ; Send data chunk &HL, BC = Length
;                push    bc
;                ld      bc, 2710h
;                call    DelayBC
;                pop     bc
;                call    SendFCB
;                call    ReceiveDataToArg
;                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func27_RandomBlockRead:  
		call SendDebugBlock         
                ;@push    bc
                ;@ld      bc, 32h
                ;@call    DelayBC
                ;@pop     bc
                ld      d, h
                call    FIFO_SendByte
                ld      d, l
                call    FIFO_SendByte
                ;@push    bc
                ;@ld      bc, 32h
                ;@call    DelayBC
                ;@pop     bc
                call    SendFCB
                call    FIFO_ReceiveByteWait
                ld      h, a
                call    FIFO_ReceiveByteWait
                ld      l, a
                call    ReceiveChunkToDMA

if DEBUG
                ld a, '@'
                out (98h), a
endif
                call    ReceiveDataToArg

if DEBUG 
                ld a, '#'
                out (98h), a
endif
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func28_RandomWriteWithZeroFill:   
                call    SendFCB
                call    Send128BytesFromDMA
                call    ReceiveDataToArg
                jp      FIFO_ReceiveByteWait
; ---------------------------------------------------------------------------

Func2A_GetDate:          
		xor a, a   
		ld h, a
		ld l, a      
                ld d, a
                ld e, a
                ret
; ---------------------------------------------------------------------------

Func2B_SetDate:                   
                ret                     ; nop
; ---------------------------------------------------------------------------

Func2C_GetTime:  
                ld      h, 0            ; Always return 0
                ld      l, h
                ld      d, h
                ld      e, h
                ret
; ---------------------------------------------------------------------------

Func2D_SetTime:                   
                ret                     ; nop
; ---------------------------------------------------------------------------

Func2E_SetResetVerifyFlag:        
                ret                     ; nop
; ---------------------------------------------------------------------------

Func2F_AbsoluteSectorRead:   
                call    FIFO_SendByte
                ld      d, e
                call    FIFO_SendByte
                ld      d, h
                call    FIFO_SendByte
                ld      d, l
                call    FIFO_SendByte
                jp      ReceiveChunkToDMA
; ---------------------------------------------------------------------------

Func30_AbsoluteSectorWrite: 
;                push    bc
;                ld      bc, 3E8h
;                call    DelayBC
;                pop     bc
;                call    FIFO_SendByte
;                ld      d, e
;                call    FIFO_SendByte
;                ld      d, h
;                call    FIFO_SendByte
;                ld      d, l
;                call    FIFO_SendByte
;                push    bc
;                ld      bc, 2710h
;                call    DelayBC
;                pop     bc
;                ld      de, 200h
;                ld      a, h
;                call    Mul_DE_by_A
;                ld      b, h
;                ld      c, l
;                ld      hl, (CurrentDMAAddr)
;                call    bdos_SendDataChunk ; Send data chunk &HL, BC = Length
                ret

FuncXX_NoOperation:
		ret

SendWord:
		ld 	d, h
		call    FIFO_SendByte
		ld 	d, l
		call 	FIFO_SendByte
                ;@push    bc
                ;@ld      bc, DELAY_SENDWORD
                ;@call    DelayBC
                ;@pop     bc
		ret

SendDebugBlock:
		exx
		ld hl, (CurrentDMAAddr)
		;ld hl, ($fffe)
		;in a, (0a8h)
		;ld l, a
		call SendWord
		ld hl, 0
		add hl, sp
		call SendWord
		ld	hl, (STORESP)
		call SendWord
		push ix
		ld   ix, (STORESP)
		ld   l, (ix+0)
		ld   h, (ix+1)
		call SendWord
		ld   l, (ix+2)
		ld   h, (ix+3)
		call SendWord
		ld   l, (ix+4)
		ld   h, (ix+5)
		call SendWord
		pop  ix
		exx
		ret


SendFCB:                          
                exx
                ld      hl, (SaveDE)
                ld      bc, SIZEOF_FCB
                call    bdos_SendDataChunk ; Send data chunk &HL, BC = Length
                exx
                ret

ReceiveDataToArg:                 
                                        
                exx
                ld      hl, (SaveDE)
                call    ReceiveDataChunk ; Receive data chunk to &HL, size not returned
                exx
                ret

; ****************************************
; * Send128BytesFromDMA 
; **************************************** 
Send128BytesFromDMA:              
                exx
                ld      hl, (CurrentDMAAddr)
                ld      bc, 80h ; 'А'
                call    bdos_SendDataChunk ; Send data chunk &HL, BC = Length
                exx
                ret


; ****************************************
; * ReceiveChunkToDMA 
; **************************************** 
ReceiveChunkToDMA:                
                                        
                exx
                ld      hl, (CurrentDMAAddr)
                call    ReceiveDataChunk ; Receive data chunk to &HL, size not returned
                exx
                ret

; ****************************************
; * FIFO_SendByte 
; **************************************** 
FIFO_SendByte:
                ld      a, 5
                out     (9), a
FIFO_SendByte_waitrx:                     
                in      a, (0Ch)
                and     41h ; 'A'
                cp      40h ; '@'
                jr      nz, FIFO_SendByte_waitrx
                ld      a, d
                out     (0Eh), a
                ret

; ****************************************
; * FIFO_ReceiveByteWait 
; **************************************** 
FIFO_ReceiveByteWait:             
                ld      a, 3
                out     (9), a
FIFO_rx_wait:                             
                in      a, (0Ch)
                ;and     83h     1000 0011
                ;cp      80h     1000 0000
                ;jr      nz, FIFO_rx_wait
                bit 7, a
                jr z, FIFO_rx_wait
                and 3
                jr nz, FIFO_rx_wait
                in      a, (0Eh)
                ret

; ****************************************
; * FIFO_ReceiveByteImmediate 
; **************************************** 
FIFO_ReceiveByteImmediate:        
                ld      a, 3
                out     (9), a
                ld      b, 0FFh
;                in      a, (0Ch)
;                and     83h
;                cp      80h
;                jr      z, FIFO_ReceiveByteImmediate_wtf
;FIFO_ReceiveByteImmediate_wtf:
                in      a, (0Eh)
                ret

; Send data chunk &HL, BC = Length
bdos_SendDataChunk:
                ld      d, b
                call    FIFO_SendByte
                ld      d, c
                call    FIFO_SendByte

bdos_SendDataChunk_sendloop: 
                ;@push    bc
                ;@ld      bc, 32h
                ;@call    DelayBC
                ;@pop     bc
                ld      d, (hl)
                call    FIFO_SendByte
                inc     hl
                dec     bc
                ld      a, b
                or      c
                jr      nz, bdos_SendDataChunk_sendloop
                ret


; Receive data chunk to &HL, size not returned
ReceiveDataChunk:                 
                call    FIFO_ReceiveByteWait
                ld      b, a
                call    FIFO_ReceiveByteWait
                ld      c, a

                ; check for zero sized chunks
                or b
                ret z
                ;
if DEBUG
                push hl
                push bc
                push de
                ld h, b
                ld l, c
                call DispHLhex
                pop de
                pop bc
                pop hl

                ld      a, '('
                out     (98h), a        ; VRAM data read/write
endif

ReceiveDataChunk_recvloop:
                call    FIFO_ReceiveByteWait
                ld      (hl), a
                inc     hl
                dec     bc

if DEBUG
                ld a, c
                or a
                jr nz, nodebugprint
                ld      a, '.'
                out     (98h), a        ; VRAM data read/write
nodebugprint:          
endif
                ld      a, b
                or      c
                jr      nz, ReceiveDataChunk_recvloop

if DEBUG
                ld      a, ')'
                out     (98h), a        ; VRAM data read/write
endif
                ret
; ****************************************
; * WaitUntilByteReceived 
; **************************************** 
WaitUntilByteReceived:            
                                        ; E972
                ex      af, af'
                exx

WaitUntilByteReceived_waitrx:                               
                call    FIFO_ReceiveByteImmediate
                or      a
                jr      nz, WaitUntilByteReceived_waitrx
                ex      af, af'
                exx
                ret

; ****************************************
; * DelayBC 
; **************************************** 
DelayBC:                          
                                        
                dec     bc
                ld      a, b
                or      c
                ret     z
                jr      DelayBC

; ****************************************
; * Mul_DE_by_A 
; **************************************** 
Mul_DE_by_A:                      
                ld      hl, 0
                ld      c, 8

rbmulmm:                                    
                add     hl, hl
                rla
                jr      nc, rbmulll
                add     hl, de

rbmulll:                                    
                adc     a, 0
                dec     c
                jr      nz, rbmulmm
                ret

; ---------------------------------------------------------------------------
ADR1_SendersNumber:db 0           
                                        
SaveDE:   		dw 0                    
                        
SaveHL:			dw 0                 
SaveBC:			dw 0
Ret1:			dw 0
Ret2:			dw 0
;CAPST:    db 0                    
                                        ; RestoreCAPS:caps_light_on ...
DispatchTable:dw Func0_ProgramTerminate
                                        
                dw Func1_ConsoleInput ; CHSNS Tests the status of the keyboard buffer
                dw Func2_ConsoleOutput ; CHSNS Tests the status of the keyboard buffer
                                        ; Z-flag set if buffer is filled
                dw Func3_AUXin    ; AUX in
                dw Func4_AUXout
                dw Func5_PrinterOutput
                dw Func6_ConsoleIO
                dw Func7_ConsoleInput
                dw Func8_ConsoleInputNoEcho
                dw Func9_StringOutput
                dw FuncA_BufferedLineInput
                dw FuncB_ConsoleStatus ; CHSNS Tests the status of the keyboard buffer
                dw FuncC_VersionNumber
                dw FuncD_DiskReset ; Set DMA to 0x0080
                dw FuncE_SelectDisk
                dw FuncF_OpenFile
                dw Func10_CloseFile
                dw Func11_SearchFirst
                dw Func12_SearchNext
                dw Func13_DeleteFile
                dw Func14_SequentialRead
                dw Func15_SequentialWrite
                dw Func16_CreateFile
                dw Func17_RenameFile
                dw Func18_GetLoginVector
                dw Func19_GetCurrentDrive
                dw Func1A_SetDMA
                dw Func1B_GetAllocInfo
                dw FuncXX_NoOperation
                dw FuncXX_NoOperation
                dw FuncXX_NoOperation
                dw FuncXX_NoOperation
                dw FuncXX_NoOperation
                dw Func21_RandomRead
                dw Func22_RandomWrite
                dw Func23_GetFileSize
                dw Func24_SetRandomRecord
                dw FuncXX_NoOperation
                dw Func26_RandomBlockWrite
                dw Func27_RandomBlockRead
                dw Func28_RandomWriteWithZeroFill
                dw FuncXX_NoOperation
                dw Func2A_GetDate ; Always return 0
                dw Func2B_SetDate ; nop
                dw Func2C_GetTime ; Always return 0
                dw Func2D_SetTime ; nop
                dw Func2E_SetResetVerifyFlag ; nop
                dw Func2F_AbsoluteSectorRead
                dw Func30_AbsoluteSectorWrite

Init:		call PatchBIOSCalls
		ret

PatchBIOSCalls:
		ld hl, ($0001) 	; points to dc03: jmp WARMBOOT
		ld bc, 3
		add hl, bc
		; HL points to CONST vector

		ex de, hl 		; DE = BIOS Jump Table
		ld hl, PatchJumpTable
		ld bc, PatchJumpTable_End - PatchJumpTable
		ldir
		ret

PatchJumpTable:	;jp WARMBOOT
		jp CONST 				; DC0F
		jp CONIN 				; DC2C
		jp CONOUT 				; DC43
		;
PatchJumpTable_End:

WARMBOOT:	jp $

CONST:			
                ld		(STORESP), sp
                ld 		sp, $DC00 
                call CONST_unsafe
		ld 		sp, (STORESP)
		ret

CONST_unsafe:
                push ix
                push iy
                rst 	30h   			; BREAKX check Ctrl+STOP 
                db 	70h			; CY when Ctrl+STOP pressed
                dw 	00B7h

                jr 	nc, CONST_1 	       ; STOP was not pressed

                ld a, 3 			; STOP was pressed
                ld ($f336), a 			; Set F336 and F337 to 3
                ld ($f337), a 			; which is a STOP trait
                and a 				; clear zero flag
                jr CONST_EXIT 			; return

CONST_1:		ld a, ($f336) 		; F336 != 0 ==> F337 is valid
		and a 				; check validity
		ld a, ($f337)  			; preload value in a
		jr nz, CONST_EXIT 		; yes, valid, exit using value in a
						; no value in f336/f337:
		rst 	30h  			; CHSNS 
		db 		70h 		; Tests the status of the keyboard buffer
		dw 		009Ch 		; Z-flag set if buffer is empty
		jr z, CONST_EXIT 		; zero set, return 
						; else
		ld a, $ff 			; set validity flag in F336
		ld ($f336), a 

		rst 	30h 			; CHGET (Waiting)
		db 		70h  		; Retrieve character from buffer
		dw 		009Fh 		; 
		ld ($f337), a 			; store the char in F337

CONST_EXIT: 					; and return 
		pop iy
		pop ix
		ret

CONIN:			
                ld		(STORESP), sp
                ld 		sp, $DC00 
                call CONIN_unsafe
		ld 		sp, (STORESP)
		ret  

CONIN_unsafe:
                push ix
                push iy
		push hl
		ld hl, $F336 			; keyboard status addr
		xor a 
		cp (hl) 			; check if (F336) is 0
		ld (hl), a			; clear (F336) for the next time
		inc hl 				; F337
		ld a, (hl) 			; load stored char value 
		pop hl 	
		jr nz, CONIN_EXIT 		; (F336) not zero, a has valid value
						; return

						; otherwise get the value from buf
                rst     30h                     ; CHGET (Waiting)
                db 	70h
                dw 	9Fh
CONIN_EXIT:
		pop iy
		pop ix
		ret

CONOUT:			
                ld		(STORESP), sp
                ld 		sp, $DC00 
                call CONOUT_unsafe
		ld 		sp, (STORESP)
		ret

CONOUT_unsafe:
                push ix
                push iy
		ld    a, c
                rst     30h             ; CHPUT
                db      70h
                dw 	0A2h
		pop iy
		pop ix
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
                out (98h), a
                ret

DispSpace:
	       ld a, ' '
DispCharInA:
        	out (98h), a
        	ret

DispCRLF:
                ld a, 0dh
                out (98h), a
                ld a, 0ah
                out (98h), a
                ret

DiagnosticsDeath:
                push hl
                push de
                push bc
                push af
                pop hl
                call DispHLhex
                call DispSpace
                pop hl
                call DispHLhex
                call DispSpace
                pop hl
                call DispHLhex
                call DispSpace
                pop hl
                call DispHLhex
                call DispSpace

                call DispSpace
                ld hl, (CurrentDMAAddr)	
                call DispHLhex
                call DispSpace

                ld hl, (CurrentDMAAddr)	
                ld d, $ff
ddloop:
                ld c, (hl)
                call OutHex8
                call DispSpace
                inc hl
                dec d
                jp nz,ddloop
                jr $


	       include 'inputline.inc'

