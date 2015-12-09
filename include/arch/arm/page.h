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
#define VM_STACK_TOP    KERNEL_VMA

typedef unsigned int    pde_t;
typedef unsigned int    pte_t;

#define ARM_VM_DIR_ENTRIES      4096
#define ARM_VM_PT_ENTRIES       256
/* size of a directory */
#define ARM_PGD_SIZE    0x4000
/* size of a page table */
#define ARM_PT_SIZE     0x1000
/* size of a physical page */
#define ARM_PG_SIZE     0x1000
/* size of the memory that a page table maps */
#define ARM_PT_MEMSIZE  (PG_SIZE * ARCH_VM_DIR_ENTRIES)

/* struct page_directory */
typedef struct {
    /* physical address of page dir */
    pde_t * phys_addr;
    /* virtual address of page dir */
    pde_t * vir_addr;

    /* virtual address of all page tables */
    pte_t * vir_pts[ARM_VM_DIR_ENTRIES];
} pgdir_t;

#define ARM_PG_PRESENT          (1 << 1)
#define ARM_PG_C                (1 << 3)
#define ARM_PG_SUPER            (0x1 << 4)
#define ARM_PG_USER             (0x3 << 4)
#define ARM_PG_TEX0             (1 << 6)
#define ARM_PG_TEX1             (1 << 7)
#define ARM_PG_TEX2             (1 << 8)
#define ARM_PG_RO               (1 << 9)
#define ARM_PG_RW               0
#define ARM_PG_CACHED           (ARM_PG_TEX2 | ARM_PG_TEX0 | ARM_PG_C )

#define ARM_SECTION_SIZE        (1024 * 1024) /* 1 MB section */
/* Big page (1MB section) specific flags. */
#define ARM_VM_SECTION          (1 << 1)  /* 1MB section */
#define ARM_VM_SECTION_PRESENT      (1 << 1)  /* Section is present */
#define ARM_VM_SECTION_B        (1 << 2)  /* B Bit */
#define ARM_VM_SECTION_C        (1 << 3)  /* C Bit */
#define ARM_VM_SECTION_DOMAIN       (0xF << 5) /* Domain Number */
#define ARM_VM_SECTION_SUPER        (0x1 << 10) /* Super access only AP[1:0] */
#define ARM_VM_SECTION_USER     (0x3 << 10) /* Super/User access AP[1:0] */
#define ARM_VM_SECTION_TEX0     (1 << 12) /* TEX[0] */
#define ARM_VM_SECTION_TEX1     (1 << 13) /* TEX[1] */
#define ARM_VM_SECTION_TEX2     (1 << 14) /* TEX[2] */
#define ARM_VM_SECTION_RO       (1 << 15)   /* Read only access AP[2] */
#define ARM_VM_SECTION_SHAREABLE    (1 << 16)  /* Shareable */
#define ARM_VM_SECTION_NOTGLOBAL    (1 << 17)  /* Not Global */
#define ARM_VM_SECTION_WTWB (ARM_VM_SECTION_TEX2 | ARM_VM_SECTION_TEX0 | ARM_VM_SECTION_C)
#define ARM_VM_SECTION_CACHED ARM_VM_SECTION_WTWB

#define ARM_VM_SECTION_SHIFT    20
#define ARM_VM_SECTION_MASK     (~((1 << ARM_VM_SECTION_SHIFT) - 1))

#define ARM_VM_PAGEDIR      (1 << 0)  /* Page directory */
#define ARM_VM_PDE_PRESENT  (1 << 0)  /* Page directory is present */
#define ARM_VM_PDE_DOMAIN   (0xF << 5) /* Domain Number */
#define ARM_VM_PDE_SHIFT    10
#define ARM_VM_PDE_MASK     (~((1 << ARM_VM_PDE_SHIFT) - 1))

#define ARM_VM_ADDR_MASK        0xFFFFF000
#define ARM_VM_OFFSET_MASK      0x00000FFF
#define ARM_VM_OFFSET_MASK_1MB  0x000FFFFF /* physical address */

/* ARM pagefault status bits */
#define ARM_PF_ALIGN          0x01 /* Pagefault caused by Alignment fault */
#define ARM_PF_IMAINT         0x04 /* Caused by Instruction cache
                      maintenance fault */
#define ARM_PF_TTWALK_L1ABORT 0x0c /* Caused by Synchronous external abort
                    * on translation table walk (Level 1)
                    */
#define ARM_PF_TTWALK_L2ABORT 0x0e /* Caused by Synchronous external abort
                    * on translation table walk (Level 2)
                    */
#define ARM_PF_TTWALK_L1PERR  0x1c /* Caused by Parity error
                    * on translation table walk (Level 1)
                    */
#define ARM_PF_TTWALK_L2PERR  0x1e /* Caused by Parity error
                    * on translation table walk (Level 2)
                    */
#define ARM_PF_L1TRANS        0x05 /* Caused by Translation fault (Level 1)
                    */
#define ARM_PF_L2TRANS        0x07 /* Caused by Translation fault (Level 2)
                    */
#define ARM_PF_L1ACCESS       0x03 /* Caused by Access flag fault (Level 1)
                    */
#define ARM_PF_L2ACCESS       0x06 /* Caused by Access flag fault (Level 2)
                    */
#define ARM_PF_L1DOMAIN       0x09 /* Caused by Domain fault (Level 1)
                    */
#define ARM_PF_L2DOMAIN       0x0b /* Caused by Domain fault (Level 2)
                    */
#define ARM_PF_L1PERM         0x0d /* Caused by Permission fault (Level 1)
                    */
#define ARM_PF_L2PERM         0x0f /* Caused by Permission fault (Level 2)
                    */
#define ARM_PF_DEBUG          0x02 /* Caused by Debug event */
#define ARM_PF_ABORT          0x08 /* Caused by Synchronous external abort
                    */
#define ARM_PF_TLB_CONFLICT   0x10 /* Caused by TLB conflict abort
                    */

#define ARM_PF_W      (1<<11)  /* Caused by write (otherwise read) */
#define ARM_PF_FS4    (1<<10)  /* Fault status (bit 4) */
#define ARM_PF_FS3_0   0xf     /* Fault status (bits 3:0) */

#define ARM_PF_FS(s) \
    ((((s) & ARM_PF_FS4) >> 6) | ((s) & ARM_PF_FS3_0))

#define ARM_PF_PROT(e)   ((ARM_PF_FS(e) == ARM_PF_L1PERM) \
             || (ARM_PF_FS(e) == ARM_PF_L2PERM))
#define ARM_PF_NOPAGE(e) (!ARM_PF_PROT(e))
#define ARM_PF_WRITE(e)  ((e) & ARM_PF_W)

#define PAGE_ALIGN              0x1000
#define PG_SIZE                 0x1000      /* 4kB */

#define ARCH_VM_DIR_ENTRIES     ARM_VM_DIR_ENTRIES
#define ARCH_VM_PT_ENTRIES      ARM_VM_PT_ENTRIES
#define ARCH_VM_ADDR_MASK       ARM_VM_ADDR_MASK

#define ARCH_PG_PRESENT         ARM_PG_PRESENT
#define ARCH_PG_USER            ARM_PG_USER
#define ARCH_PG_RW              ARM_PG_RW
#define ARCH_PG_RO              ARM_PG_RO
#define ARCH_PG_BIGPAGE         ARM_VM_SECTION

#define ARCH_PGD_SIZE           ARM_PGD_SIZE
#define ARCH_PT_SIZE            ARM_PT_SIZE
#define ARCH_PG_SIZE            ARM_PG_SIZE
#define ARCH_BIG_PAGE_SIZE      ARM_SECTION_SIZE

#define ARCH_PF_PROT(x)         ARM_PF_PROT(x)
#define ARCH_PF_NOPAGE(x)       ARM_PF_NOPAGE(x)
#define ARCH_PF_WRITE(x)        ARM_PF_WRITE(x)

#define ARCH_VM_ADDRESS(pde, pte, offset)   (pde * ARM_PT_MEMSIZE + pte * ARM_PG_SIZE + offset)

#define ARCH_PTE(v)             (((unsigned long)(v) >> 12) & 0xff)
#define ARCH_PDE(x)             ((unsigned long)(x) / ARM_SECTION_SIZE)

#endif
