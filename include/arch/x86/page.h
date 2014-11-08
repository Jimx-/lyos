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

#ifndef _PAGE_H_
#define _PAGE_H_

#define KERNEL_VMA      0xf0000000
#define VMALLOC_END     0xf7c00000
#define FIXMAP_START    VMALLOC_END
#define FIXMAP_END      0xfec00000
#define VM_STACK_TOP    KERNEL_VMA

typedef unsigned int    pde_t;
typedef unsigned int    pte_t;

#define I386_VM_DIR_ENTRIES     1024

#define I386_VM_ADDR_MASK       0xfffff000
#define I386_VM_OFFSET_MASK     (~I386_VM_ADDR_MASK)
#define I386_VM_ADDR_MASK_BIG   0xffc00000
#define I386_VM_OFFSET_MASK_BIG 0x003fffff

/* struct page_directory */
typedef struct {
    /* physical address of page dir */
    pde_t * phys_addr;
    /* virtual address of page dir */
    pde_t * vir_addr;

    /* virtual address of all page tables */
    pte_t * vir_pts[I386_VM_DIR_ENTRIES];
} pgdir_t;

/* size of a directory */
#define PGD_SIZE    0x1000
/* size of a page table */
#define PT_SIZE     0x1000
/* size of a physical page */
#define PG_SIZE     0x1000
/* size of the memory that a page table maps */
#define PT_MEMSIZE  (PG_SIZE * I386_VM_DIR_ENTRIES)
/* size of a big page */
#define PG_BIG_SIZE PT_MEMSIZE

#define PG_PRESENT  0x001
#define PG_RO       0x000 
#define PG_RW       0x002
#define PG_USER     0x004
#define PG_BIGPAGE  0x080
#define PG_GLOBAL   (1L<< 8)

#define PAGE_ALIGN  0x1000

#define I386_CR0_WP     0x00010000  /* Enable paging */
#define I386_CR0_PG     0x80000000  /* Enable paging */

#define I386_CR4_PSE    0x00000010  /* Page size extensions */
#define I386_CR4_PAE    0x00000020  /* Physical addr extens. */
#define I386_CR4_MCE    0x00000040  /* Machine check enable */
#define I386_CR4_PGE    0x00000080  /* Global page flag enable */

#define I386_PF_PROT(x)         ((x) & PG_PRESENT)
#define I386_PF_NOPAGE(x)       (!I386_PF_PROT(x))

PUBLIC void setup_paging(pde_t * pgd, pte_t * pt, int kpts);
PUBLIC void enable_paging();
PUBLIC void disable_paging();
PUBLIC int read_cr2();
PUBLIC int read_cr3();
PUBLIC void reload_cr3();

#define ARCH_PG_PRESENT         PG_PRESENT
#define ARCH_PG_RW              PG_RW
#define ARCH_PG_BIGPAGE         PG_BIGPAGE
#define ARCH_PG_USER            PG_USER
#define ARCH_PG_GLOBAL          PG_GLOBAL

#define ARCH_VM_DIR_ENTRIES     I386_VM_DIR_ENTRIES
#define ARCH_VM_ADDR_MASK       I386_VM_ADDR_MASK
#define ARCH_VM_OFFSET_MASK     I386_VM_OFFSET_MASK
#define ARCH_VM_ADDR_MASK_BIG   I386_VM_ADDR_MASK_BIG
#define ARCH_VM_OFFSET_MASK_BIG I386_VM_OFFSET_MASK_BIG
#define ARCH_PF_PROT(x)         I386_PF_PROT(x)
#define ARCH_PF_NOPAGE(x)       I386_PF_NOPAGE(x)

#define ARCH_PDE(x)     ((unsigned long)(x) >> 22 & 0x03FF)
#define ARCH_PTE(x)     ((unsigned long)(x) >> 12 & 0x03FF)

#define ARCH_VM_ADDRESS(pde, pte, offset)   (pde * PT_MEMSIZE + pte * PG_SIZE + offset)

#define ARCH_PGD_SIZE        PGD_SIZE
#define ARCH_PG_SIZE         PG_SIZE
#define ARCH_BIG_PAGE_SIZE   PG_BIG_SIZE

#endif
