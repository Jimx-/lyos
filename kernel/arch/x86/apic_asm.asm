bits 32

%include "sconst.inc"

[section .text]

global	apic_hwint00
global	apic_hwint01
global	apic_hwint02
global	apic_hwint03
global	apic_hwint04
global	apic_hwint05
global	apic_hwint06
global	apic_hwint07
global	apic_hwint08
global	apic_hwint09
global  apic_hwint10
global  apic_hwint11
global  apic_hwint12
global  apic_hwint13
global  apic_hwint14
global  apic_hwint15
global  apic_hwint16
global  apic_hwint17
global  apic_hwint18
global  apic_hwint19
global  apic_hwint20
global  apic_hwint21
global  apic_hwint22
global  apic_hwint23
global  apic_hwint24
global  apic_hwint25
global  apic_hwint26
global  apic_hwint27
global  apic_hwint28
global  apic_hwint29
global  apic_hwint30
global  apic_hwint31
global  apic_hwint32
global  apic_hwint33
global  apic_hwint34
global  apic_hwint35
global  apic_hwint36
global  apic_hwint37
global  apic_hwint38
global  apic_hwint39
global  apic_hwint40
global  apic_hwint41
global  apic_hwint42
global  apic_hwint43
global  apic_hwint44
global  apic_hwint45
global  apic_hwint46
global  apic_hwint47
global  apic_hwint48
global  apic_hwint49
global  apic_hwint50
global  apic_hwint51
global  apic_hwint52
global  apic_hwint53
global  apic_hwint54
global  apic_hwint55
global  apic_hwint56
global  apic_hwint57
global  apic_hwint58
global  apic_hwint59
global  apic_hwint60
global  apic_hwint61
global  apic_hwint62
global  apic_hwint63
global  apic_timer_int_handler
global  apic_spurious_intr
global  apic_error_intr

extern  save
extern  stop_context
extern  switch_to_user
extern  irq_handle
extern  idle_stop
extern  apic_eoi_write
extern  clock_handler
extern  apic_spurious_int_handler
extern  apic_error_int_handler

; interrupt and exception - hardware interrupt
; ---------------------------------
%macro	apic_hwint      1
    cmp dword [esp + 4], SELECTOR_KERNEL_CS		; Test if this interrupt is triggered in kernel
    je .1
    call	save
    push    esi
    call    stop_context
    pop     esi
    push	%1						; `.
    call	irq_handle	;  | Call the interrupt handler
    pop	ecx							; /
    jmp switch_to_user
.1:
    pushad
    call idle_stop
    push	%1			; `.
    call	irq_handle	;  | 中断处理程序
    pop	ecx			; /
    CLEAR_IF	dword [esp + 40]
    popad
    iret
%endmacro

%macro	lapic_int       1
    cmp dword [esp + 4], SELECTOR_KERNEL_CS		; Test if this interrupt is triggered in kernel
    je .1
    call	save
    push    esi
    call    stop_context
    pop     esi
    call	%1
    call    apic_eoi_write
    jmp     switch_to_user
.1:
    pushad
    call    idle_stop
    call	%1
    call    apic_eoi_write
    CLEAR_IF	dword [esp + 40]
    popad
    iret
%endmacro

ALIGN   16
apic_timer_int_handler:
    lapic_int	clock_handler
ALIGN   16
apic_spurious_intr:
    lapic_int	apic_spurious_int_handler
ALIGN   16
apic_error_intr:
    lapic_int	apic_error_int_handler

ALIGN	16
apic_hwint00:
    apic_hwint	0
ALIGN	16
apic_hwint01:
    apic_hwint	1
ALIGN	16
apic_hwint02:
    apic_hwint	2
ALIGN	16
apic_hwint03:
    apic_hwint	3
ALIGN	16
apic_hwint04:
    apic_hwint	4
ALIGN	16
apic_hwint05:
    apic_hwint	5
ALIGN	16
apic_hwint06:
    apic_hwint	6
ALIGN	16
apic_hwint07:
    apic_hwint	7
ALIGN	16
apic_hwint08:
    apic_hwint	8
ALIGN	16
apic_hwint09:
    apic_hwint	9
ALIGN	16
apic_hwint10:
    apic_hwint	10
ALIGN	16
apic_hwint11:
    apic_hwint	11
ALIGN	16
apic_hwint12:
    apic_hwint	12
ALIGN	16
apic_hwint13:
    apic_hwint	13
ALIGN	16
apic_hwint14:
    apic_hwint	14
ALIGN	16
apic_hwint15:
    apic_hwint	15
ALIGN	16
apic_hwint16:
    apic_hwint	16
ALIGN	16
apic_hwint17:
    apic_hwint	17
ALIGN	16
apic_hwint18:
    apic_hwint	18
ALIGN	16
apic_hwint19:
    apic_hwint	19
ALIGN	16
apic_hwint20:
    apic_hwint	20
ALIGN	16
apic_hwint21:
    apic_hwint	21
ALIGN	16
apic_hwint22:
    apic_hwint	22
ALIGN	16
apic_hwint23:
    apic_hwint	23
ALIGN	16
apic_hwint24:
    apic_hwint	24
ALIGN	16
apic_hwint25:
    apic_hwint	25
ALIGN	16
apic_hwint26:
    apic_hwint	26
ALIGN	16
apic_hwint27:
    apic_hwint	27
ALIGN	16
apic_hwint28:
    apic_hwint	28
ALIGN	16
apic_hwint29:
    apic_hwint	29
ALIGN	16
apic_hwint30:
    apic_hwint	30
ALIGN	16
apic_hwint31:
    apic_hwint	31
ALIGN	16
apic_hwint32:
    apic_hwint	32
ALIGN	16
apic_hwint33:
    apic_hwint	33
ALIGN	16
apic_hwint34:
    apic_hwint	34
ALIGN	16
apic_hwint35:
    apic_hwint	35
ALIGN	16
apic_hwint36:
    apic_hwint	36
ALIGN	16
apic_hwint37:
    apic_hwint	37
ALIGN	16
apic_hwint38:
    apic_hwint	38
ALIGN	16
apic_hwint39:
    apic_hwint	39
ALIGN	16
apic_hwint40:
    apic_hwint	40
ALIGN	16
apic_hwint41:
    apic_hwint	41
ALIGN	16
apic_hwint42:
    apic_hwint	42
ALIGN	16
apic_hwint43:
    apic_hwint	43
ALIGN	16
apic_hwint44:
    apic_hwint	44
ALIGN	16
apic_hwint45:
    apic_hwint	45
ALIGN	16
apic_hwint46:
    apic_hwint	46
ALIGN	16
apic_hwint47:
    apic_hwint	47
ALIGN	16
apic_hwint48:
    apic_hwint	48
ALIGN	16
apic_hwint49:
    apic_hwint	49
ALIGN	16
apic_hwint50:
    apic_hwint	50
ALIGN	16
apic_hwint51:
    apic_hwint	51
ALIGN	16
apic_hwint52:
    apic_hwint	52
ALIGN	16
apic_hwint53:
    apic_hwint	53
ALIGN	16
apic_hwint54:
    apic_hwint	54
ALIGN	16
apic_hwint55:
    apic_hwint	55
ALIGN	16
apic_hwint56:
    apic_hwint	56
ALIGN	16
apic_hwint57:
    apic_hwint	57
ALIGN	16
apic_hwint58:
    apic_hwint	58
ALIGN	16
apic_hwint59:
    apic_hwint	59
ALIGN	16
apic_hwint60:
    apic_hwint	60
ALIGN	16
apic_hwint61:
    apic_hwint	61
ALIGN	16
apic_hwint62:
    apic_hwint	62
ALIGN	16
apic_hwint63:
    apic_hwint	63
