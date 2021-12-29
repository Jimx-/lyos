#ifndef _ARCH_FPU_H_
#define _ARCH_FPU_H_

#include <lyos/types.h>

struct fxregs_state {
    u16 cwd; /* Control Word			*/
    u16 swd; /* Status Word			*/
    u16 twd; /* Tag Word			*/
    u16 fop; /* Last Instruction Opcode		*/
    union {
        struct {
            u64 rip; /* Instruction Pointer		*/
            u64 rdp; /* Data Pointer			*/
        };
        struct {
            u32 fip; /* FPU IP Offset			*/
            u32 fcs; /* FPU IP Selector			*/
            u32 foo; /* FPU Operand Offset		*/
            u32 fos; /* FPU Operand Selector		*/
        };
    };
    u32 mxcsr;      /* MXCSR Register State */
    u32 mxcsr_mask; /* MXCSR Mask		*/

    /* 8*16 bytes for each FP-reg = 128 bytes:			*/
    u32 st_space[32];

    /* 16*16 bytes for each XMM-reg = 256 bytes:			*/
    u32 xmm_space[64];

    u32 padding[12];

    union {
        u32 padding1[12];
        u32 sw_reserved[12];
    };

} __attribute__((aligned(16)));

#define MXCSR_DEFAULT 0x1f80

struct xstate_header {
    u64 xfeatures;
    u64 xcomp_bv;
    u64 reserved[6];
} __attribute__((packed));

#define XCOMP_BV_COMPACTED_FORMAT ((u64)1 << 63)

struct xregs_state {
    struct fxregs_state i387;
    struct xstate_header header;
    u8 extended_state_area[0];
} __attribute__((packed, aligned(64)));

union fpregs_state {
    struct fxregs_state fxsave;
    struct xregs_state xsave;
};

#endif // _ARCH_FPU_H_
