/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ARCH_PAGE_H_
#define _ARCH_PAGE_H_

#include <lyos/config.h>

#define LOWMEM_END 0x30000000
#define KERNEL_VMA CONFIG_KERNEL_VMA

#ifdef CONFIG_64BIT

#define KERNEL_VIRT_SIZE (-KERNEL_VMA)
#define PKMAP_SIZE       (KERNEL_VIRT_SIZE >> 1)
#define PKMAP_START      (KERNEL_VMA - PKMAP_SIZE)
#define PKMAP_END        KERNEL_VMA

#define PAGE_OFFSET   0x1000000000UL
#define VMALLOC_START 0x2000000000UL
#define VMALLOC_END   0x3000000000UL
#define VM_STACK_TOP  0x4000000000UL
#endif

#define RISCV_PG_SHIFT (12)
#define RISCV_PG_SIZE  (1UL << RISCV_PG_SHIFT)
#define RISCV_PG_MASK  (~(RISCV_PG_SIZE - 1))

#define RISCV_PG_PFN_SHIFT 10

#define RISCV_VM_DIR_ENTRIES (RISCV_PG_SIZE / sizeof(pde_t))
#define RISCV_VM_PT_ENTRIES  (RISCV_PG_SIZE / sizeof(pte_t))

#ifndef __ASSEMBLY__

#ifdef CONFIG_64BIT
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif

typedef struct {
    unsigned long pde;
} pde_t;

typedef struct {
    unsigned long pte;
} pte_t;

typedef struct {
    unsigned long pgprot;
} pgprot_t;

#define pde_val(x) ((x).pde)
#define __pde(x)   ((pde_t){(x)})

#define pte_val(x) ((x).pte)
#define __pte(x)   ((pte_t){(x)})

#define pgprot_val(x) ((x).pgprot)
#define __pgprot(x)   ((pgprot_t){(x)})

/* struct page_directory */
typedef struct {
    /* physical address of page dir */
    phys_bytes phys_addr;
    /* virtual address of page dir */
    pde_t* vir_addr;

    /* virtual address of all page tables */
    pte_t* vir_pts[RISCV_VM_DIR_ENTRIES];
} pgdir_t;

extern unsigned long va_pa_offset;

#endif

#define ARCH_VM_DIR_ENTRIES RISCV_VM_DIR_ENTRIES
#define ARCH_VM_PT_ENTRIES  RISCV_VM_PT_ENTRIES

#define ARCH_PG_PRESENT _RISCV_PG_PRESENT
#define ARCH_PG_USER    _RISCV_PG_USER
#define ARCH_PG_RW      (_RISCV_PG_READ | _RISCV_PG_WRITE)
#define ARCH_PG_RO      _RISCV_PG_READ
#define ARCH_PG_BIGPAGE 0

#define ARCH_PGD_SIZE  RISCV_PGD_SIZE
#define ARCH_PGD_SHIFT RISCV_PGD_SHIFT
#define ARCH_PGD_MASK  RISCV_PGD_MASK

#ifndef __PAGETABLE_PMD_FOLDED
#define ARCH_VM_PMD_ENTRIES RISCV_VM_PMD_ENTRIES

#define ARCH_PMD_SIZE  RISCV_PMD_SIZE
#define ARCH_PMD_SHIFT RISCV_PMD_SHIFT
#define ARCH_PMD_MASK  RISCV_PMD_MASK
#endif

#define ARCH_PG_SIZE  RISCV_PG_SIZE
#define ARCH_PG_SHIFT RISCV_PG_SHIFT
#define ARCH_PG_MASK  RISCV_PG_MASK

#define ARCH_BIG_PAGE_SIZE RISCV_PGD_SIZE

#define ARCH_VM_ADDRESS(pde, pte, offset) \
    ((pde << RISCV_PGD_SHIFT) | (pte << RISCV_PG_SHIFT) | offset)

#define ARCH_PTE(v) \
    (((unsigned long)(v) >> RISCV_PG_SHIFT) & (ARCH_VM_PT_ENTRIES - 1))
#define ARCH_PDE(v) \
    (((unsigned long)(v) >> RISCV_PGD_SHIFT) & (ARCH_VM_DIR_ENTRIES - 1))

#define __pa(x) ((phys_bytes)(x)-va_pa_offset)
#define __va(x) ((void*)((unsigned long)(x) + va_pa_offset))

#ifndef __ASSEMBLY__

static inline unsigned long virt_to_phys(void* x) { return __pa(x); }

#ifdef __kernel__
static inline void* phys_to_virt(unsigned long x) { return __va(x); }
#else
extern void* phys_to_virt(unsigned long x);
#endif

#endif

#endif
