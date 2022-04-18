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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/page.h>
#include <asm/pagetable.h>
#include <errno.h>
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "asm/cpulocals.h"
#include <lyos/cpufeature.h>
#include <asm/fixmap.h>
#include <asm/sysreg.h>

#include <libfdt/libfdt.h>

unsigned long va_pa_offset;

#define NO_BLOCK_MAPPINGS BIT(0)
#define NO_CONT_MAPPINGS  BIT(1)
#define ALLOC_PHYS_MEM    BIT(2)

static pte_t bm_pt[ARCH_VM_PT_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));
static pmd_t bm_pmd[ARCH_VM_PMD_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));
static pud_t bm_pud[ARCH_VM_PUD_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));

phys_bytes pg_alloc_pages(kinfo_t* pk, unsigned int nr_pages)
{
    int i;
    vir_bytes size = nr_pages * ARCH_PG_SIZE;

    for (i = pk->memmaps_count - 1; i >= 0; i--) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];

        if (entry->type != KINFO_MEMORY_AVAILABLE) continue;

        if (!(entry->addr % ARCH_PG_SIZE) && (entry->len >= size)) {
            entry->addr += size;
            entry->len -= size;

            return entry->addr - size;
        }
    }

    return 0;
}

phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size)
{
    int i;

    for (i = 0; i < pk->memmaps_count; i++) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];

        if (entry->type != KINFO_MEMORY_AVAILABLE) continue;

        if (!(entry->addr % ARCH_PG_SIZE) && (entry->len >= size)) {
            entry->addr += size;
            entry->len -= size;

            return entry->addr - size;
        }
    }

    return 0;
}

static phys_bytes pg_alloc_pt(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_pages(pk, 1);
    void* pt;

    if (!phys_addr) {
        panic("failed to allocate page table page");
    }

    pt = pte_set_fixmap(phys_addr);

    memset(pt, 0, ARCH_PG_SIZE);

    pte_clear_fixmap();

    return phys_addr;
}

static inline pud_t* fixmap_pude(unsigned long addr)
{
    pde_t* pde;

    pde = pgd_offset(swapper_pg_dir, addr);
    return pud_offset_kimg(pde, addr);
}

static inline pmd_t* fixmap_pmde(unsigned long addr)
{
    pud_t* pude = fixmap_pude(addr);
    return pmd_offset_kimg(pude, addr);
}

static inline pte_t* fixmap_pte(unsigned long addr)
{
    return &bm_pt[ARCH_PTE(addr)];
}

static void alloc_init_pte(pmd_t* pmde, unsigned long addr, unsigned long end,
                           phys_bytes phys, pgprot_t prot, int flags,
                           kinfo_t* pk)
{
    pte_t* pte;

    if (pmde_none(*pmde)) {
        pmdval_t pmdval = _ARM64_PMD_TYPE_TABLE;
        phys_bytes pt_phys;

        if (!pk) panic("page table allocation not allowed");
        pt_phys = pg_alloc_pt(pk);
        __pmde_populate(pmde, pt_phys, pmdval);
    }

    pte = pte_set_fixmap_offset(pmde, addr);
    do {
        phys_bytes ph = phys;
        if (flags & ALLOC_PHYS_MEM) ph = pg_alloc_pages(pk, 1);

        set_pte(pte, pfn_pte(ph >> ARCH_PG_SHIFT, prot));

        phys += ARCH_PG_SIZE;
    } while (pte++, addr += ARCH_PG_SIZE, addr != end);

    pte_clear_fixmap();
}

static void alloc_init_pmd(pud_t* pude, unsigned long addr, unsigned long end,
                           phys_bytes phys, pgprot_t prot, int flags,
                           kinfo_t* pk)
{
    unsigned long next;
    pmd_t* pmde;

    if (pude_none(*pude)) {
        pudval_t pudval = _ARM64_PUD_TYPE_TABLE;
        phys_bytes pmd_phys;

        if (!pk) panic("page table allocation not allowed");
        pmd_phys = pg_alloc_pt(pk);
        __pude_populate(pude, pmd_phys, pudval);
    }

    pmde = pmd_set_fixmap_offset(pude, addr);
    do {
        next = pmd_addr_end(addr, end);

        if (((addr | next | phys) & ~ARCH_PMD_MASK) == 0 &&
            !(flags & NO_BLOCK_MAPPINGS)) {
            set_pmde(pmde,
                     pfn_pmd(phys >> ARCH_PG_SHIFT, mk_pmd_sect_prot(prot)));
        } else {
            alloc_init_pte(pmde, addr, next, phys, prot, flags, pk);
        }
        phys += next - addr;
    } while (pmde++, addr = next, addr != end);

    pmd_clear_fixmap();
}

static void alloc_init_pud(pde_t* pde, unsigned long addr, unsigned long end,
                           phys_bytes phys, pgprot_t prot, int flags,
                           kinfo_t* pk)
{
    unsigned long next;
    pud_t* pude;

    if (pde_none(*pde)) {
        pdeval_t pdeval = _ARM64_PGD_TYPE_TABLE;
        phys_bytes pud_phys;

        if (!pk) panic("page table allocation not allowed");
        pud_phys = pg_alloc_pt(pk);
        __pde_populate(pde, pud_phys, pdeval);
    }

    pude = pud_set_fixmap_offset(pde, addr);
    do {
        next = pud_addr_end(addr, end);

        if (pud_sect_supported() &&
            ((addr | next | phys) & ~ARCH_PUD_MASK) == 0 &&
            !(flags & NO_BLOCK_MAPPINGS)) {
            set_pude(pude,
                     pfn_pud(phys >> ARCH_PG_SHIFT, mk_pud_sect_prot(prot)));
        } else {
            alloc_init_pmd(pude, addr, next, phys, prot, flags, pk);
        }

        phys += next - addr;
    } while (pude++, addr = next, addr != end);

    pud_clear_fixmap();
}

static void __create_pgd_mapping(pde_t* pgd_page, phys_bytes phys,
                                 vir_bytes virt, phys_bytes size, pgprot_t prot,
                                 int flags, kinfo_t* pk)
{
    unsigned long addr, end, next;
    pde_t* pde = pgd_offset(pgd_page, virt);

    phys &= ARCH_PG_MASK;
    addr = virt & ARCH_PG_MASK;
    end = roundup(virt + size, ARCH_PG_SIZE);

    do {
        next = pgd_addr_end(addr, end);
        alloc_init_pud(pde, addr, next, phys, prot, flags, pk);
        phys += next - addr;
    } while (pde++, addr = next, addr != end);
}

static void create_mapping_noalloc(phys_bytes phys, vir_bytes virt,
                                   phys_bytes size, pgprot_t prot)
{
    __create_pgd_mapping(swapper_pg_dir, phys, virt, size, prot,
                         NO_CONT_MAPPINGS, NULL);
}

void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t* pk)
{
    int flags = NO_BLOCK_MAPPINGS | NO_CONT_MAPPINGS;

    if (phys_addr == 0) flags |= ALLOC_PHYS_MEM;

    __create_pgd_mapping(mm_pg_dir, phys_addr, (vir_bytes)vir_addr,
                         vir_end - vir_addr, ARM64_PG_SHARED_EXEC, flags, pk);
}

void early_fixmap_init(void)
{
    pde_t* pde;
    pud_t* pude;
    pmd_t* pmde;
    vir_bytes addr = FIXADDR_START;

    pde = pgd_offset(swapper_pg_dir, addr);
    if (pde_none(*pde))
        __pde_populate(pde, __pa_symbol(bm_pud), _ARM64_PGD_TYPE_TABLE);
    pude = fixmap_pude((unsigned long)addr);

    if (pude_none(*pude))
        __pude_populate(pude, __pa_symbol(bm_pmd), _ARM64_PUD_TYPE_TABLE);
    pmde = fixmap_pmde(addr);

    __pmde_populate(pmde, __pa_symbol(bm_pt), _ARM64_PMD_TYPE_TABLE);
}

void* fixmap_remap_fdt(phys_bytes dt_phys, int* size, pgprot_t prot)
{
    const unsigned long dt_virt_base = __fix_to_virt(FIX_FDT);
    int offset;
    void* dt_virt;

    if (!dt_phys || dt_phys % 8) return NULL;

    offset = dt_phys % SWAPPER_BLOCK_SIZE;
    dt_virt = (void*)dt_virt_base + offset;

    create_mapping_noalloc(rounddown(dt_phys, SWAPPER_BLOCK_SIZE), dt_virt_base,
                           SWAPPER_BLOCK_SIZE, prot);

    if (fdt_magic(dt_virt) != FDT_MAGIC) return NULL;

    *size = fdt_totalsize(dt_virt);
    if (*size > (2 << 20)) return NULL;

    if (offset + *size > SWAPPER_BLOCK_SIZE) {
        create_mapping_noalloc(
            rounddown(dt_phys, SWAPPER_BLOCK_SIZE), dt_virt_base,
            roundup(offset + *size, SWAPPER_BLOCK_SIZE), prot);
    }

    return dt_virt;
}

void __set_fixmap(enum fixed_address idx, phys_bytes phys, pgprot_t prot)
{
    unsigned long addr = __fix_to_virt(idx);
    pte_t* pte;

    pte = fixmap_pte(addr);

    if (pgprot_val(prot)) {
        set_pte(pte, pfn_pte(phys >> ARCH_PG_SHIFT, prot));
    } else {
        pte_clear(pte);
        flush_tlb();
    }
}

void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end)
{
    int i = 0;

    while (i < pk->memmaps_count) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];
        u64 mmap_end = entry->addr + entry->len;

        if (entry->addr > end) break;

        if ((start >= entry->addr && start < mmap_end) ||
            (end >= entry->addr && end < mmap_end)) {

            if (start == entry->addr && end == mmap_end) {
                pk->memmaps_count--;
                if (i < pk->memmaps_count)
                    memmove(&pk->memmaps[i], &pk->memmaps[i + 1],
                            (pk->memmaps_count - i) * sizeof(*entry));
            } else if (start > entry->addr && end < mmap_end) {
                if (pk->memmaps_count == KINFO_MAXMEMMAP)
                    panic("Memory map table full");

                memmove(&pk->memmaps[i + 2], &pk->memmaps[i + 1],
                        (pk->memmaps_count - i - 1) * sizeof(*entry));
                pk->memmaps[i + 1] = *entry;
                entry->len = start - entry->addr;
                pk->memmaps[i + 1].addr = end;
                pk->memmaps[i + 1].len = mmap_end - end;
                pk->memmaps_count++;
                i += 2;
            } else if (start > entry->addr) {
                entry->len = start - entry->addr;
                i++;
            } else {
                entry->addr = end;
                entry->len = mmap_end - end;
                i++;
            }
        } else
            i++;
    }
}

void paging_init(void)
{
    struct kinfo_mmap_entry* mmap;
    int i;

    va_pa_offset = PAGE_OFFSET;

    for (i = 0, mmap = kinfo.memmaps; i < kinfo.memmaps_count; i++, mmap++) {
        phys_bytes start = mmap->addr;
        phys_bytes end = mmap->addr + mmap->len;

        start &= ARCH_PUD_MASK;
        end = roundup(end, ARCH_PUD_SIZE);

        if (start >= end) break;

        __create_pgd_mapping(swapper_pg_dir, start, __phys_to_virt(start),
                             end - start, ARM64_PG_KERNEL, 0, &kinfo);
    }
}

void switch_address_space(struct proc* p)
{
    get_cpulocal_var(pt_proc) = p;
    flush_tlb();
    write_sysreg(p->seg.ttbr_phys, ttbr0_el1);
    isb();
}
