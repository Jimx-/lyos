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

#define KERNEL_VMA      0xc0000000
#define VMALLOC_START   0xf0000000
#define VMALLOC_END     0xf7c00000
#define FIXMAP_START    VMALLOC_END
#define FIXMAP_END      0xffffe000
#define VM_STACK_TOP    KERNEL_VMA

typedef unsigned int    pde_t;
typedef unsigned int    pte_t;

#define I386_VM_DIR_ENTRIES     1024

#define I386_VM_ADDR_MASK       0xfffff000

/* struct page_directory */
struct page_directory {
    /* physical address of page dir */
    pde_t * phys_addr;
    /* virtual address of page dir */
    pde_t * vir_addr;

    /* virtual address of all page tables */
    pte_t * vir_pts[I386_VM_DIR_ENTRIES];
};

/* size of a directory */
#define PGD_SIZE    0x1000
/* size of a page table */
#define PT_SIZE     0x1000
/* size of a physical page */
#define PG_SIZE     0x1000
/* size of the memory that a page table maps */
#define PT_MEMSIZE  (PG_SIZE * 1024)

#define PG_PRESENT  0x001 
#define PG_RW       0x002
#define PG_USER     0x004

#define PAGE_ALIGN  0x1000

#define I386_CR0_PG 0x80000000

#define I386_PF_PROT(x)         ((x) & PG_PRESENT)
#define I386_PF_NOPAGE(x)       (!I386_PF_PROT(x))

PUBLIC void setup_paging(pde_t * pgd, pte_t * pt, int kpts);
PUBLIC void switch_address_space(pde_t * pgd);
PUBLIC void enable_paging();
PUBLIC void disable_paging();
PUBLIC int read_cr2();
PUBLIC int read_cr3();
PUBLIC void reload_cr3();

#define ARCH_VM_DIR_ENTRIES     I386_VM_DIR_ENTRIES
#define ARCH_VM_ADDR_MASK       I386_VM_ADDR_MASK
#define ARCH_PF_PROT(x)         I386_PF_PROT(x)
#define ARCH_PF_NOPAGE(x)       I386_PF_NOPAGE(x)

#define ARCH_PDE(x)     ((unsigned long)(x) >> 22 & 0x03FF)
#define ARCH_PTE(x)     ((unsigned long)(x) >> 12 & 0x03FF)

#define ARCH_VM_ADDRESS(pde, pte, offset)   (pde * PT_MEMSIZE + pte * PG_SIZE + offset)

#endif
