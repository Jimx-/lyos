#ifndef _ARCH_PAGETABLE_BITS_H_
#define _ARCH_PAGETABLE_BITS_H_

#define _RISCV_PG_PRESENT   (1 << 0)
#define _RISCV_PG_READ      (1 << 1)
#define _RISCV_PG_WRITE     (1 << 2)
#define _RISCV_PG_EXEC      (1 << 3)
#define _RISCV_PG_USER      (1 << 4)
#define _RISCV_PG_GLOBAL    (1 << 5)
#define _RISCV_PG_ACCESSED  (1 << 6)
#define _RISCV_PG_DIRTY     (1 << 7)

#define _RISCV_PG_TABLE     _RISCV_PG_PRESENT

#endif // _ARCH_PAGETABLE_BITS_H_
