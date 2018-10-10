#ifndef _ARCH_CSR_H_
#define _ARCH_CSR_H_

/* Status register flags */
#define SR_SUM  0x00040000UL    /* Supervisor may access User Memory */

/* SATP flags */
#if __riscv_xlen == 32
#define SATP_PPN     0x003FFFFFUL
#define SATP_MODE_32 0x80000000UL
#define SATP_MODE    SATP_MODE_32
#else
#define SATP_PPN     0x00000FFFFFFFFFFFUL
#define SATP_MODE_39 0x8000000000000000UL
#define SATP_MODE    SATP_MODE_39
#endif

#endif // _ARCH_CSR_H_
