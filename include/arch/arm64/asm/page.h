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
#include <lyos/const.h>

#define VA_BITS (CONFIG_ARM64_VA_BITS)

#define KERNEL_VMA (_PAGE_END(VA_BITS_MIN))

#if VA_BITS > 48
#define VA_BITS_MIN (48)
#else
#define VA_BITS_MIN (VA_BITS)
#endif

#define _PAGE_END(va) (-(_AC(1, UL) << ((va)-1)))

/* Memory types. */
#define MT_NORMAL        0
#define MT_NORMAL_TAGGED 1
#define MT_NORMAL_NC     2
#define MT_DEVICE_nGnRnE 3
#define MT_DEVICE_nGnRE  4

#define IDMAP_PGTABLE_LEVELS (CONFIG_PGTABLE_LEVELS - 1)
#define INIT_PGTABLE_LEVELS  (CONFIG_PGTABLE_LEVELS - 1)

#define IDMAP_DIR_SIZE (IDMAP_PGTABLE_LEVELS * ARCH_PG_SIZE)
#define INIT_DIR_SIZE  (INIT_PGTABLE_LEVELS * ARCH_PG_SIZE)

#define ARM64_HW_PGTABLE_LEVEL_SHIFT(n) ((ARCH_PG_SHIFT - 3) * (4 - (n)) + 3)

#define ARM64_PG_SHIFT      CONFIG_ARM64_PAGE_SHIFT
#define ARM64_PG_SIZE       (_AC(1, UL) << ARM64_PG_SHIFT)
#define ARM64_PG_MASK       (~(ARM64_PG_SIZE - 1))
#define ARM64_VM_PT_ENTRIES (1 << (ARM64_PG_SHIFT - 3))

#ifndef __ASSEMBLY__

typedef struct {
    unsigned long pde;
} pde_t;

#if CONFIG_PGTABLE_LEVELS > 3
typedef struct {
    unsigned long pud;
} pud_t;

#define pud_val(x) ((x).pud)
#define __pud(x)   ((pud_t){(x)})
#endif

#if CONFIG_PGTABLE_LEVELS > 2
typedef struct {
    unsigned long pmd;
} pmd_t;

#define pmd_val(x) ((x).pmd)
#define __pmd(x)   ((pmd_t){(x)})
#endif

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
} pgdir_t;

#endif

#define ARCH_PG_SIZE       ARM64_PG_SIZE
#define ARCH_PG_SHIFT      ARM64_PG_SHIFT
#define ARCH_PG_MASK       ARM64_PG_MASK
#define ARCH_VM_PT_ENTRIES ARM64_VM_PT_ENTRIES

#if CONFIG_PGTABLE_LEVELS > 2
#define ARCH_PMD_SHIFT      ARM64_HW_PGTABLE_LEVEL_SHIFT(2)
#define ARCH_PMD_SIZE       (_AC(1, UL) << ARCH_PMD_SHIFT)
#define ARCH_PMD_MASK       (~(ARCH_PMD_SIZE - 1))
#define ARCH_VM_PMD_ENTRIES ARCH_VM_PT_ENTRIES
#endif

#if CONFIG_PGTABLE_LEVELS > 3
#define ARCH_PUD_SHIFT      ARM64_HW_PGTABLE_LEVEL_SHIFT(1)
#define ARCH_PUD_SIZE       (_AC(1, UL) << ARCH_PUD_SHIFT)
#define ARCH_PUD_MASK       (~(ARCH_PUD_SIZE - 1))
#define ARCH_VM_PUD_ENTRIES ARCH_VM_PT_ENTRIES
#endif

#define ARCH_PGD_SHIFT      ARM64_HW_PGTABLE_LEVEL_SHIFT(4 - CONFIG_PGTABLE_LEVELS)
#define ARCH_PGD_SIZE       (_AC(1, UL) << ARCH_PGD_SHIFT)
#define ARCH_PGD_MASK       (~(ARCH_PGD_SIZE - 1))
#define ARCH_VM_DIR_ENTRIES (1 << (VA_BITS - ARCH_PGD_SHIFT))

#define _ARM64_PMD_TYPE_TABLE (_AT(pmdval_t, 3) << 0)
#define _ARM64_PMD_TYPE_SECT  (_AT(pmdval_t, 1) << 0)

#define _ARM64_SECT_VALID  (_AT(pmdval_t, 1) << 0)
#define _ARM64_SECT_USER   (_AT(pmdval_t, 1) << 6)
#define _ARM64_SECT_RDONLY (_AT(pmdval_t, 1) << 7)
#define _ARM64_SECT_S      (_AT(pmdval_t, 3) << 8)
#define _ARM64_SECT_AF     (_AT(pmdval_t, 1) << 10)
#define _ARM64_SECT_NG     (_AT(pmdval_t, 1) << 11)
#define _ARM64_SECT_CONT   (_AT(pmdval_t, 1) << 52)
#define _ARM64_SECT_PXN    (_AT(pmdval_t, 1) << 53)
#define _ARM64_SECT_UXN    (_AT(pmdval_t, 1) << 54)

#define SWAPPER_TABLE_SHIFT ARCH_PUD_SHIFT
#define SWAPPER_BLOCK_SHIFT ARCH_PMD_SHIFT
#define SWAPPER_BLOCK_SIZE  ARCH_PMD_SIZE

/*
 * TCR flags.
 */
#define TCR_T0SZ_OFFSET 0
#define TCR_T1SZ_OFFSET 16
#define TCR_T0SZ(x)     ((UL(64) - (x)) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ(x)     ((UL(64) - (x)) << TCR_T1SZ_OFFSET)
#define TCR_TxSZ(x)     (TCR_T0SZ(x) | TCR_T1SZ(x))
#define TCR_TxSZ_WIDTH  6
#define TCR_T0SZ_MASK   (((UL(1) << TCR_TxSZ_WIDTH) - 1) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ_MASK   (((UL(1) << TCR_TxSZ_WIDTH) - 1) << TCR_T1SZ_OFFSET)

#define TCR_EPD0_SHIFT  7
#define TCR_EPD0_MASK   (UL(1) << TCR_EPD0_SHIFT)
#define TCR_IRGN0_SHIFT 8
#define TCR_IRGN0_MASK  (UL(3) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_NC    (UL(0) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBWA  (UL(1) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WT    (UL(2) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBnWA (UL(3) << TCR_IRGN0_SHIFT)

#define TCR_EPD1_SHIFT  23
#define TCR_EPD1_MASK   (UL(1) << TCR_EPD1_SHIFT)
#define TCR_IRGN1_SHIFT 24
#define TCR_IRGN1_MASK  (UL(3) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_NC    (UL(0) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBWA  (UL(1) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WT    (UL(2) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBnWA (UL(3) << TCR_IRGN1_SHIFT)

#define TCR_IRGN_NC    (TCR_IRGN0_NC | TCR_IRGN1_NC)
#define TCR_IRGN_WBWA  (TCR_IRGN0_WBWA | TCR_IRGN1_WBWA)
#define TCR_IRGN_WT    (TCR_IRGN0_WT | TCR_IRGN1_WT)
#define TCR_IRGN_WBnWA (TCR_IRGN0_WBnWA | TCR_IRGN1_WBnWA)
#define TCR_IRGN_MASK  (TCR_IRGN0_MASK | TCR_IRGN1_MASK)

#define TCR_ORGN0_SHIFT 10
#define TCR_ORGN0_MASK  (UL(3) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_NC    (UL(0) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBWA  (UL(1) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WT    (UL(2) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBnWA (UL(3) << TCR_ORGN0_SHIFT)

#define TCR_ORGN1_SHIFT 26
#define TCR_ORGN1_MASK  (UL(3) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_NC    (UL(0) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBWA  (UL(1) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WT    (UL(2) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBnWA (UL(3) << TCR_ORGN1_SHIFT)

#define TCR_ORGN_NC    (TCR_ORGN0_NC | TCR_ORGN1_NC)
#define TCR_ORGN_WBWA  (TCR_ORGN0_WBWA | TCR_ORGN1_WBWA)
#define TCR_ORGN_WT    (TCR_ORGN0_WT | TCR_ORGN1_WT)
#define TCR_ORGN_WBnWA (TCR_ORGN0_WBnWA | TCR_ORGN1_WBnWA)
#define TCR_ORGN_MASK  (TCR_ORGN0_MASK | TCR_ORGN1_MASK)

#define TCR_SH0_SHIFT 12
#define TCR_SH0_MASK  (UL(3) << TCR_SH0_SHIFT)
#define TCR_SH0_INNER (UL(3) << TCR_SH0_SHIFT)

#define TCR_SH1_SHIFT 28
#define TCR_SH1_MASK  (UL(3) << TCR_SH1_SHIFT)
#define TCR_SH1_INNER (UL(3) << TCR_SH1_SHIFT)
#define TCR_SHARED    (TCR_SH0_INNER | TCR_SH1_INNER)

#define TCR_TG0_SHIFT 14
#define TCR_TG0_MASK  (UL(3) << TCR_TG0_SHIFT)
#define TCR_TG0_4K    (UL(0) << TCR_TG0_SHIFT)
#define TCR_TG0_64K   (UL(1) << TCR_TG0_SHIFT)
#define TCR_TG0_16K   (UL(2) << TCR_TG0_SHIFT)

#define TCR_TG1_SHIFT 30
#define TCR_TG1_MASK  (UL(3) << TCR_TG1_SHIFT)
#define TCR_TG1_16K   (UL(1) << TCR_TG1_SHIFT)
#define TCR_TG1_4K    (UL(2) << TCR_TG1_SHIFT)
#define TCR_TG1_64K   (UL(3) << TCR_TG1_SHIFT)

#define TCR_IPS_SHIFT 32
#define TCR_IPS_MASK  (UL(7) << TCR_IPS_SHIFT)
#define TCR_A1        (UL(1) << 22)
#define TCR_ASID16    (UL(1) << 36)
#define TCR_TBI0      (UL(1) << 37)
#define TCR_TBI1      (UL(1) << 38)
#define TCR_HA        (UL(1) << 39)
#define TCR_HD        (UL(1) << 40)
#define TCR_TBID1     (UL(1) << 52)
#define TCR_NFD0      (UL(1) << 53)
#define TCR_NFD1      (UL(1) << 54)
#define TCR_E0PD0     (UL(1) << 55)
#define TCR_E0PD1     (UL(1) << 56)
#define TCR_TCMA0     (UL(1) << 57)
#define TCR_TCMA1     (UL(1) << 58)

#endif
