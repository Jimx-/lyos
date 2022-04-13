#ifndef _ARCH_SCONST_H_
#define _ARCH_SCONST_H_

#include <lyos/config.h>

#define REG_SIZE 8

#define P_STACKBASE 0
#define X0REG       P_STACKBASE
#define X2REG       (X0REG + REG_SIZE * 2)
#define X4REG       (X2REG + REG_SIZE * 2)
#define X6REG       (X4REG + REG_SIZE * 2)
#define X8REG       (X6REG + REG_SIZE * 2)
#define X10REG      (X8REG + REG_SIZE * 2)
#define X12REG      (X10REG + REG_SIZE * 2)
#define X14REG      (X12REG + REG_SIZE * 2)
#define X16REG      (X14REG + REG_SIZE * 2)
#define X18REG      (X16REG + REG_SIZE * 2)
#define X20REG      (X18REG + REG_SIZE * 2)
#define X22REG      (X20REG + REG_SIZE * 2)
#define X24REG      (X22REG + REG_SIZE * 2)
#define X26REG      (X24REG + REG_SIZE * 2)
#define X28REG      (X26REG + REG_SIZE * 2)
#define FPREG       (X28REG + REG_SIZE)
#define LRREG       (FPREG + REG_SIZE)
#define SPREG       (LRREG + REG_SIZE)
#define PCREG       (SPREG + REG_SIZE)

#define P_PSTATE   (PCREG + REG_SIZE)
#define P_KERNELSP (P_PSTATE + REG_SIZE)
#define P_ORIGX0   (P_KERNELSP + REG_SIZE)
#define P_CPU      (P_ORIGX0 + REG_SIZE)

#define CurrentEL_EL1 (1 << 2)
#define CurrentEL_EL2 (2 << 2)

/*
 * PSR bits
 */
#define PSR_MODE_EL0t 0x00000000
#define PSR_MODE_EL1t 0x00000004
#define PSR_MODE_EL1h 0x00000005
#define PSR_MODE_EL2t 0x00000008
#define PSR_MODE_EL2h 0x00000009
#define PSR_MODE_EL3t 0x0000000c
#define PSR_MODE_EL3h 0x0000000d
#define PSR_MODE_MASK 0x0000000f

/* AArch32 CPSR bits */
#define PSR_MODE32_BIT 0x00000010

/* AArch64 SPSR bits */
#define PSR_F_BIT      0x00000040
#define PSR_I_BIT      0x00000080
#define PSR_A_BIT      0x00000100
#define PSR_D_BIT      0x00000200
#define PSR_BTYPE_MASK 0x00000c00
#define PSR_SSBS_BIT   0x00001000
#define PSR_PAN_BIT    0x00400000
#define PSR_UAO_BIT    0x00800000
#define PSR_DIT_BIT    0x01000000
#define PSR_TCO_BIT    0x02000000
#define PSR_V_BIT      0x10000000
#define PSR_C_BIT      0x20000000
#define PSR_Z_BIT      0x40000000
#define PSR_N_BIT      0x80000000

/* clang-format off */

.irp	n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
    wx\n	.req	w\n
.endr

.macro adr_l, dst, sym
    adrp \dst, \sym
    add \dst, \dst, : lo12 :\sym
.endm

.macro  ldr_l, dst, sym, tmp=
    .ifb    \tmp
    adrp    \dst, \sym
    ldr     \dst, [\dst, :lo12:\sym]
    .else
    adrp    \tmp, \sym
    ldr     \dst, [\tmp, :lo12:\sym]
    .endif
.endm

.macro  str_l, src, sym, tmp
    adrp    \tmp, \sym
    str     \src, [\tmp, :lo12:\sym]
.endm

.macro	mov_q, reg, val
    .if (((\val) >> 31) == 0 || ((\val) >> 31) == 0x1ffffffff)
    movz	\reg, :abs_g1_s:\val
    .else
    .if (((\val) >> 47) == 0 || ((\val) >> 47) == 0x1ffff)
    movz	\reg, :abs_g2_s:\val
    .else
    movz	\reg, :abs_g3:\val
    movk	\reg, :abs_g2_nc:\val
    .endif
    movk	\reg, :abs_g1_nc:\val
    .endif
    movk	\reg, :abs_g0_nc:\val
.endm

/* clang-format on */

#define MAIR_EL1_SET                                           \
    (MAIR_ATTRIDX(MAIR_ATTR_DEVICE_nGnRnE, MT_DEVICE_nGnRnE) | \
     MAIR_ATTRIDX(MAIR_ATTR_DEVICE_nGnRE, MT_DEVICE_nGnRE) |   \
     MAIR_ATTRIDX(MAIR_ATTR_NORMAL_NC, MT_NORMAL_NC) |         \
     MAIR_ATTRIDX(MAIR_ATTR_NORMAL, MT_NORMAL) |               \
     MAIR_ATTRIDX(MAIR_ATTR_NORMAL, MT_NORMAL_TAGGED))

#ifdef CONFIG_ARM64_64K_PAGES
#define TCR_TG_FLAGS TCR_TG0_64K | TCR_TG1_64K
#elif defined(CONFIG_ARM64_16K_PAGES)
#define TCR_TG_FLAGS TCR_TG0_16K | TCR_TG1_16K
#else /* CONFIG_ARM64_4K_PAGES */
#define TCR_TG_FLAGS TCR_TG0_4K | TCR_TG1_4K
#endif

#define TCR_SMP_FLAGS TCR_SHARED

#define TCR_CACHE_FLAGS TCR_IRGN_WBWA | TCR_ORGN_WBWA

#endif
