#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#include <asm/pagetable-bits.h>

#define _RISCV_PG_BASE  (_RISCV_PG_PRESENT | _RISCV_PG_ACCESSED | _RISCV_PG_USER | _RISCV_PG_DIRTY)

#define RISCV_PG_NONE       __pgprot(0)
#define RISCV_PG_READ       __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ)
#define RISCV_PG_WRITE      __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_WRITE)
#define RISCV_PG_EXEC       __pgprot(_RISCV_PG_BASE | _RISCV_PG_EXEC)
#define RISCV_PG_EXEC_READ  __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_EXEC)
#define RISCV_PG_EXEC_WRITE __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_WRITE | _RISCV_PG_EXEC)

#define _RISCV_PG_KERNEL    (_RISCV_PG_PRESENT | _RISCV_PG_READ | _RISCV_PG_WRITE | _RISCV_PG_ACCESSED | _RISCV_PG_DIRTY)
#define RISCV_PG_KERNEL     __pgprot(_RISCV_PG_KERNEL)
#define RISCV_PG_KERNLE_EXEC    __pgprot(_RISCV_PG_KERNEL | _RISCV_PG_EXEC)

PRIVATE inline pde_t pfn_pde(unsigned long pfn, pgprot_t prot)
{
	return __pde((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

PRIVATE inline pmd_t pfn_pmd(unsigned long pfn, pgprot_t prot)
{
	return __pmd((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

PRIVATE inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
	return __pte((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

#endif // _ARCH_PAGETABLE_H_
