
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                               boot.asm
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;                                                    	Jimx 2012
; ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


	org  07c00h

BaseOfStack		equ	07c00h	; Base of stack

%include	"load.inc"
;================================================================================================

	jmp short LABEL_START		; Start to boot.
	nop				

%include	"fat12hdr.inc"
%include	"compile.inc"

LABEL_START:	
	mov	ax, cs
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	mov	sp, BaseOfStack

	; clean screen
	mov	ax, 0600h		; AH = 6,  AL = 0h
	mov	bx, 0700h		
	mov	cx, 0			
	mov	dx, 0184fh		
	int	10h			; int 10h

	mov	dh, 0			; "Booting  "
	call	DispStr			
	
	; reset floppy drive
	xor	ah, ah
	xor	dl, dl
	int	13h	

	; load loader from the second sector
	mov	ax, LOADER_SEG
	mov	es, ax			; es <- LOADER_SEG
	mov	bx, LOADER_OFF		; bx <- LOADER_OFF
	mov	ax, 1			; ax <- 2nd sector

LABEL_LOADING_LOADER:
	push	ax	
	push	bx		
	mov	ah, 0Eh			
	mov	al, '.'		
	mov	bl, 0Fh	
	int	10h			
	pop	bx			
	pop	ax

	cmp	word [wLoaderSectors], 0	
	jz	LABEL_LOADED_LOADER		
	mov	cl, 1		; cl <- how many sectors
	call	ReadSector
	inc ax
	add	bx, [BPB_BytsPerSec]
	dec word [wLoaderSectors]
	jmp LABEL_LOADING_LOADER

LABEL_LOADED_LOADER:
	mov	dh, 1			; "Ready."
	call	DispStr			;

; *****************************************************************************************************
	jmp	LOADER_SEG:LOADER_OFF	; jump to loader.bin
; *****************************************************************************************************



;============================================================================
;Variables
;----------------------------------------------------------------------------
wLoaderSectors	dw	LOADERSECTS

;============================================================================
;Strings
;----------------------------------------------------------------------------
LoaderFileName		db	"LOADER  BIN", 0
MessageLength		equ	9
BootMessage:		db	"Booting  "
Message1		db	"Ready.   "
Message2		db	"No LOADER"
;============================================================================


;----------------------------------------------------------------------------
; DispStr
;----------------------------------------------------------------------------
;
;	Display a string. 
;	dh: string index
DispStr:
	mov	ax, MessageLength
	mul	dh
	add	ax, BootMessage
	;ES:BP = string's address
	mov	bp, ax
	mov	ax, ds
	mov	es, ax
	mov	cx, MessageLength	; CX <- length of string
	mov	ax, 01301h		; AH = 13,  AL = 01h
	mov	bx, 0007h
	mov	dl, 0
	int	10h
	ret


;----------------------------------------------------------------------------
; ReadSector
;----------------------------------------------------------------------------
;
;	read sector
;	cl: number of sectors
;	ax: start sector
;	es:bx: buffer
ReadSector:
	push	bp
	mov	bp, sp
	sub	esp, 2	

	mov	byte [bp-2], cl
	push	bx
	mov	bl, [BPB_SecPerTrk]
	div	bl			
	inc	ah		
	mov	cl, ah		
	mov	dh, al	
	shr	al, 1	
	mov	ch, al	
	and	dh, 1	
	pop	bx	
	mov	dl, [BS_DrvNum]	
.GoOnReading:
	mov	ah, 2		
	mov	al, byte [bp-2]	
	int	13h
	jc	.GoOnReading	

	add	esp, 2
	pop	bp

	ret

times 	510-($-$$)	db	0	; Make sure boot.bin is 512 bytes
dw 	0xaa55				; boot flag
