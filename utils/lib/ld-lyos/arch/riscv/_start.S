    #include "asm.h"
    
    .global _start
    .global ldso_bind_entry
    .extern ldso_main

    .section .text

    .align	4
    .globl	_start
    .hidden	_start
    .type	_start,@function
_start:
    mv s0, a0
    mv s1, a1
    mv s2, a2

.L0:	auipc	gp, %pcrel_hi(_GLOBAL_OFFSET_TABLE_)
	ld	t0, %pcrel_lo(.L0)(gp) # &_DYNAMIC
.L1:	auipc	a0, %pcrel_hi(_DYNAMIC)
	addi	a0, a0, %pcrel_lo(.L1)
	sub	s3, a0, t0
	move	a1, s3
	call	ldso_relocate_nonplt_self
    
    mv a0, a2
    call __lyos_init

    mv a0, s0
    mv a1, s1
    mv a2, s2
    call ldso_main
    mv t0, a0

    mv a0, s0
    mv a1, s1
    mv a2, s2

    jr t0

#define	XCALLFRAME_SIZE		(12*REG_SIZE)
#define	XCALLFRAME_RA		(8*REG_SIZE)
#define	XCALLFRAME_A7		(7*REG_SIZE)
#define	XCALLFRAME_A6		(6*REG_SIZE)
#define	XCALLFRAME_A5		(5*REG_SIZE)
#define	XCALLFRAME_A4		(4*REG_SIZE)
#define	XCALLFRAME_A3		(3*REG_SIZE)
#define	XCALLFRAME_A2		(2*REG_SIZE)
#define	XCALLFRAME_A1		(1*REG_SIZE)
#define	XCALLFRAME_A0		(0*REG_SIZE)

    .align	4
    .globl	ldso_bind_entry
    .hidden	ldso_bind_entry
    .type	ldso_bind_entry,@function
ldso_bind_entry:
    addi    sp, sp, -(XCALLFRAME_SIZE)
    REG_S	a0,  XCALLFRAME_A0(sp)
	REG_S	a1,  XCALLFRAME_A1(sp)
	REG_S	a2,  XCALLFRAME_A2(sp)
	REG_S	a3,  XCALLFRAME_A3(sp)
	REG_S	a4,  XCALLFRAME_A4(sp)
	REG_S	a5,  XCALLFRAME_A5(sp)
	REG_S	a6,  XCALLFRAME_A6(sp)
	REG_S	a7,  XCALLFRAME_A7(sp)
	REG_S	ra,  XCALLFRAME_RA(sp)

    mv  a0, t0
    mv  a1, t1

    slli a1, a1, 1
    add  a1, a1, t1

    call ldso_bind
    mv  t0, a0

    REG_L	a0, XCALLFRAME_A0(sp)
	REG_L	a1, XCALLFRAME_A1(sp)
	REG_L	a2, XCALLFRAME_A2(sp)
	REG_L	a3, XCALLFRAME_A3(sp)
	REG_L	a4, XCALLFRAME_A4(sp)
	REG_L	a5, XCALLFRAME_A5(sp)
	REG_L	a6, XCALLFRAME_A6(sp)
	REG_L	a7, XCALLFRAME_A7(sp)
	REG_L	ra, XCALLFRAME_RA(sp)		
	addi	sp, sp, XCALLFRAME_SIZE
	jr	t0
