#ifndef _ARCH_CSR_H_
#define _ARCH_CSR_H_

/* Status register flags */
#define SR_SIE  0x00000002UL    /* Superviosr Interrupt Enable */
#define SR_SPP  0x00000100UL    /* Previously Supervisor */
#define SR_SUM  0x00040000UL    /* Supervisor may access User Memory */

#define SR_FS           0x00006000UL
#define SR_FS_OFF       0x00000000UL
#define SR_FS_INITIAL   0x00002000UL
#define SR_FS_CLEAN     0x00004000UL
#define SR_FS_DIRTY     0x00006000UL

#ifndef CONFIG_64BIT
#define SR_SD   0x80000000UL /* FS/XS dirty */
#else
#define SR_SD   0x8000000000000000UL /* FS/XS dirty */
#endif

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

/* cause */
#define EXC_INST_MISALIGNED     0
#define EXC_INST_ACCESS         1
#define EXC_BREAKPOINT          3
#define EXC_LOAD_ACCESS         5
#define EXC_STORE_ACCESS        7
#define EXC_SYSCALL             8
#define EXC_INST_PAGE_FAULT     12
#define EXC_LOAD_PAGE_FAULT     13
#define EXC_STORE_PAGE_FAULT    15

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " #csr			\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#endif // _ARCH_CSR_H_
