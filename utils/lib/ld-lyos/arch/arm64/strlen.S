    .section .text
    .globl strlen

/* Assumptions:
 *
 * ARMv8-a, AArch64, unaligned accesses, min page size 4k.
 */

#define L(label) .L ## label

/* Arguments and results.  */
#define srcin		x0
#define len		x0

/* Locals and temporaries.  */
#define src		x1
#define data1		x2
#define data2		x3
#define has_nul1	x4
#define has_nul2	x5
#define tmp1		x4
#define tmp2		x5
#define tmp3		x6
#define tmp4		x7
#define zeroones	x8

    /* NUL detection works on the principle that (X - 1) & (~X) & 0x80
       (=> (X - 1) & ~(X | 0x7f)) is non-zero iff a byte is zero, and
       can be done in parallel across the entire word. A faster check
       (X - 1) & 0x80 is zero for non-NUL ASCII characters, but gives
       false hits for characters 129..255.	*/

#define REP8_01 0x0101010101010101
#define REP8_7f 0x7f7f7f7f7f7f7f7f
#define REP8_80 0x8080808080808080

#define MIN_PAGE_SIZE 4096

strlen:
    and	tmp1, srcin, MIN_PAGE_SIZE - 1
    mov	zeroones, REP8_01
    cmp	tmp1, MIN_PAGE_SIZE - 16
    b.gt	L(page_cross)
    ldp	data1, data2, [srcin]
#ifdef __AARCH64EB__
    /* For big-endian, carry propagation (if the final byte in the
       string is 0x01) means we cannot use has_nul1/2 directly.
       Since we expect strings to be small and early-exit,
       byte-swap the data now so has_null1/2 will be correct.  */
    rev	data1, data1
    rev	data2, data2
#endif
    sub	tmp1, data1, zeroones
    orr	tmp2, data1, REP8_7f
    sub	tmp3, data2, zeroones
    orr	tmp4, data2, REP8_7f
    bics	has_nul1, tmp1, tmp2
    bic	has_nul2, tmp3, tmp4
    ccmp	has_nul2, 0, 0, eq
    beq	L(main_loop_entry)

    /* Enter with C = has_nul1 == 0.  */
    csel	has_nul1, has_nul1, has_nul2, cc
    mov	len, 8
    rev	has_nul1, has_nul1
    clz	tmp1, has_nul1
    csel	len, xzr, len, cc
    add	len, len, tmp1, lsr 3
    ret

    /* The inner loop processes 32 bytes per iteration and uses the fast
       NUL check.  If we encounter non-ASCII characters, use a second
       loop with the accurate NUL check.  */
    .p2align 4
L(main_loop_entry):
    bic	src, srcin, 15
    sub	src, src, 16
L(main_loop):
    ldp	data1, data2, [src, 32]!
L(page_cross_entry):
    sub	tmp1, data1, zeroones
    sub	tmp3, data2, zeroones
    orr	tmp2, tmp1, tmp3
    tst	tmp2, zeroones, lsl 7
    bne	1f
    ldp	data1, data2, [src, 16]
    sub	tmp1, data1, zeroones
    sub	tmp3, data2, zeroones
    orr	tmp2, tmp1, tmp3
    tst	tmp2, zeroones, lsl 7
    beq	L(main_loop)
    add	src, src, 16
1:
    /* The fast check failed, so do the slower, accurate NUL check.	 */
    orr	tmp2, data1, REP8_7f
    orr	tmp4, data2, REP8_7f
    bics	has_nul1, tmp1, tmp2
    bic	has_nul2, tmp3, tmp4
    ccmp	has_nul2, 0, 0, eq
    beq	L(nonascii_loop)

    /* Enter with C = has_nul1 == 0.  */
L(tail):
#ifdef __AARCH64EB__
    /* For big-endian, carry propagation (if the final byte in the
       string is 0x01) means we cannot use has_nul1/2 directly.  The
       easiest way to get the correct byte is to byte-swap the data
       and calculate the syndrome a second time.  */
    csel	data1, data1, data2, cc
    rev	data1, data1
    sub	tmp1, data1, zeroones
    orr	tmp2, data1, REP8_7f
    bic	has_nul1, tmp1, tmp2
#else
    csel	has_nul1, has_nul1, has_nul2, cc
#endif
    sub	len, src, srcin
    rev	has_nul1, has_nul1
    add	tmp2, len, 8
    clz	tmp1, has_nul1
    csel	len, len, tmp2, cc
    add	len, len, tmp1, lsr 3
    ret

L(nonascii_loop):
    ldp	data1, data2, [src, 16]!
    sub	tmp1, data1, zeroones
    orr	tmp2, data1, REP8_7f
    sub	tmp3, data2, zeroones
    orr	tmp4, data2, REP8_7f
    bics	has_nul1, tmp1, tmp2
    bic	has_nul2, tmp3, tmp4
    ccmp	has_nul2, 0, 0, eq
    bne	L(tail)
    ldp	data1, data2, [src, 16]!
    sub	tmp1, data1, zeroones
    orr	tmp2, data1, REP8_7f
    sub	tmp3, data2, zeroones
    orr	tmp4, data2, REP8_7f
    bics	has_nul1, tmp1, tmp2
    bic	has_nul2, tmp3, tmp4
    ccmp	has_nul2, 0, 0, eq
    beq	L(nonascii_loop)
    b	L(tail)

    /* Load 16 bytes from [srcin & ~15] and force the bytes that precede
       srcin to 0x7f, so we ignore any NUL bytes before the string.
       Then continue in the aligned loop.  */
L(page_cross):
    bic	src, srcin, 15
    ldp	data1, data2, [src]
    lsl	tmp1, srcin, 3
    mov	tmp4, -1
#ifdef __AARCH64EB__
    /* Big-endian.	Early bytes are at MSB.	 */
    lsr	tmp1, tmp4, tmp1	/* Shift (tmp1 & 63).  */
#else
    /* Little-endian.  Early bytes are at LSB.  */
    lsl	tmp1, tmp4, tmp1	/* Shift (tmp1 & 63).  */
#endif
    orr	tmp1, tmp1, REP8_80
    orn	data1, data1, tmp1
    orn	tmp2, data2, tmp1
    tst	srcin, 8
    csel	data1, data1, tmp4, eq
    csel	data2, data2, tmp2, eq
    b	L(page_cross_entry)
