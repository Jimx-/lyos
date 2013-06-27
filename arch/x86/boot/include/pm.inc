;    This file is part of Lyos.
;
;    Lyos is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.

;    Lyos is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

;----------------------------------------------------------------------------
;       DA_  : Descriptor Attribute
;       D    : Data seg.
;       C    : Code seg.
;       S    : System seg.
;       R    : Read-only
;       RW   : Read-write
;       A    : Accessed
;----------------------------------------------------------------------------
DA_32		EQU	4000h	; 32 bit seg.
DA_LIMIT_4K	EQU	8000h	; 4 kB

DA_DPL0		EQU	  00h	; DPL = 0
DA_DPL1		EQU	  20h	; DPL = 1
DA_DPL2		EQU	  40h	; DPL = 2
DA_DPL3		EQU	  60h	; DPL = 3
;----------------------------------------------------------------------------
; 
;----------------------------------------------------------------------------
DA_DR		EQU	90h	; 存在的只读数据段类型值
DA_DRW		EQU	92h	; 存在的可读写数据段属性值
DA_DRWA		EQU	93h	; 存在的已访问可读写数据段类型值
DA_C		EQU	98h	; 存在的只执行代码段属性值
DA_CR		EQU	9Ah	; 存在的可执行可读代码段属性值
DA_CCO		EQU	9Ch	; 存在的只执行一致代码段属性值
DA_CCOR		EQU	9Eh	; 存在的可执行可读一致代码段属性值
;----------------------------------------------------------------------------
;
;----------------------------------------------------------------------------
DA_LDT		EQU	  82h	; 局部描述符表段类型值
DA_TaskGate	EQU	  85h	; 任务门类型值
DA_386TSS	EQU	  89h	; 可用 386 任务状态段类型值
DA_386CGate	EQU	  8Ch	; 386 调用门类型值
DA_386IGate	EQU	  8Eh	; 386 中断门类型值
DA_386TGate	EQU	  8Fh	; 386 陷阱门类型值
;----------------------------------------------------------------------------


SA_RPL0		EQU	0	; ┓
SA_RPL1		EQU	1	; ┣ RPL
SA_RPL2		EQU	2	; ┃
SA_RPL3		EQU	3	; ┛

SA_TIG		EQU	0	; ┓TI
SA_TIL		EQU	4	; ┛
;----------------------------------------------------------------------------


;----------------------------------------------------------------------------
; 
;----------------------------------------------------------------------------
PG_P		EQU	1	; 
PG_RWR		EQU	0	; 
PG_RWW		EQU	2	; 
PG_USS		EQU	0	; 
PG_USU		EQU	4	;
;----------------------------------------------------------------------------


; =========================================
; FLAGS - Intel 8086 Family Flags Register
; =========================================
;
;      |11|10|F|E|D|C|B|A|9|8|7|6|5|4|3|2|1|0|
;        |  | | | | | | | | | | | | | | | | '---  CF……Carry Flag
;        |  | | | | | | | | | | | | | | '---  PF……Parity Flag
;        |  | | | | | | | | | | | | '---  AF……Auxiliary Flag
;        |  | | | | | | | | | | '---  ZF……Zero Flag
;        |  | | | | | | | | | '---  SF……Sign Flag
;        |  | | | | | | | | '---  TF……Trap Flag  (Single Step)
;        |  | | | | | | | '---  IF……Interrupt Flag
;        |  | | | | | | '---  DF……Direction Flag
;        |  | | | | | '---  OF……Overflow flag
;        |  | | | '-----  IOPL……I/O Privilege Level  (286+ only)
;        |  | | '-----  NT……Nested Task Flag  (286+ only)
;        |  '-----  RF……Resume Flag (386+ only)
;        '------  VM……Virtual Mode Flag (386+ only)
;
;        ps : see   PUSHF  POPF  STI  CLI  STD  CLD
;


; Macro ------------------------------------------------------------------------------------------------------
;
; Descriptor
; usage: Descriptor Base, Limit, Attr
;        Base:  dd
;        Limit: dd (low 20 bits available)
;        Attr:  dw (lower 4 bits of higher byte are always 0)
%macro Descriptor 3
	dw	%2 & 0FFFFh				; Limit 1				(2 bytes)
	dw	%1 & 0FFFFh				; Base 1				(2 bytes)
	db	(%1 >> 16) & 0FFh			; Base 2				(1 byte)
	dw	((%2 >> 8) & 0F00h) | (%3 & 0F0FFh)	;
	db	(%1 >> 24) & 0FFh			;
%endmacro ; Total 8 bytes
;
; Gate
; usage: Gate Selector, Offset, DCount, Attr
;        Selector:  dw
;        Offset:    dd
;        DCount:    db
;        Attr:      db
%macro Gate 4
	dw	(%2 & 0FFFFh)				; Offset 1				(2 bytes)
	dw	%1					; Selector				(2 bytes)
	dw	(%3 & 1Fh) | ((%4 << 8) & 0FF00h)	; 					(2 bytes)
	dw	((%2 >> 16) & 0FFFFh)			; Offset 2				(2 bytes)
%endmacro ; Total 8 bytes
; ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
