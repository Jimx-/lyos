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
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/page.h>
#include <asm/pagetable.h>
#include <errno.h>
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/cpufeature.h>

void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end)
{
    int i;

    for (i = 0; i < pk->memmaps_count; i++) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];
        u64 mmap_end = entry->addr + entry->len;
        if (start >= entry->addr && end <= mmap_end) {
            entry->addr = end;
            entry->len = mmap_end - entry->addr;
        }
    }
}

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

static pte_t* pg_alloc_pt(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_pages(pk, 1);
    pte_t* pt = __va(phys_addr);

    memset(pt, 0, sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
    return pt;
}

#ifndef __PAGETABLE_PMD_FOLDED
static pmd_t* pg_alloc_pmd(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_pages(pk, 1);
    pmd_t* pmd = __va(phys_addr);

    memset(pmd, 0, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
    return pmd;
}
#else
static inline pmd_t* pg_alloc_pmd(kinfo_t* pk) { return NULL; }
#endif

#ifndef __PAGETABLE_PUD_FOLDED
static pud_t* pg_alloc_pud(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_pages(pk, 1);
    pud_t* pud = __va(phys_addr);

    memset(pud, 0, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES);
    return pud;
}
#else
static inline pud_t* pg_alloc_pud(kinfo_t* pk) { return NULL; }
#endif

void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t* pk)
{
    pde_t* pgd = initial_pgd;

    if (phys_addr % ARCH_PG_SIZE) phys_addr = roundup(phys_addr, ARCH_PG_SIZE);

    while (vir_addr < vir_end) {
        phys_bytes ph = phys_addr;
        if (ph == 0) ph = pg_alloc_pages(pk, 1);

        pde_t* pde = pgd_offset(pgd, (unsigned long)vir_addr);
        if (!pde_present(*pde) || pde_large(*pde)) {
            pud_t* new_pud = pg_alloc_pud(pk);
            pde_populate(pde, __pa(new_pud));
        }

        pud_t* pude = pud_offset(pde, (unsigned long)vir_addr);
        if (!pude_present(*pude) || pude_large(*pude)) {
            pmd_t* new_pmd = pg_alloc_pmd(pk);
            pude_populate(pude, __pa(new_pmd));
        }

        pmd_t* pmde = pmd_offset(pude, (unsigned long)vir_addr);
        if (!pmde_present(*pmde) || pmde_large(*pmde)) {
            pte_t* new_pt = pg_alloc_pt(pk);
            pmde_populate(pmde, __pa(new_pt));
        }

        pte_t* pte = pte_offset(pmde, (unsigned long)vir_addr);
        *pte = pfn_pte(ph >> ARCH_PG_SHIFT,
                       __pgprot(ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER));

        vir_addr += ARCH_PG_SIZE;
        if (phys_addr != 0) phys_addr += ARCH_PG_SIZE;
    }
}

/**
 * <Ring 0> Setup identity paging for kernel
 */

#ifdef CONFIG_X86_32

int pg_identity_map(pde_t* pgd_page, phys_bytes pstart, phys_bytes pend,
                    unsigned long offset)
{
    int i;
    phys_bytes phys;
    int flags = ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER | ARCH_PG_BIGPAGE;
    /* initialize page directory */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        if (i >= kinfo.kernel_start_pde && i < kinfo.kernel_end_pde)
            continue; /* don't touch kernel */
        phys = i * ARCH_BIG_PAGE_SIZE;
        pgd_page[i] = __pde(phys | flags);
    }

    return 0;
}

#else

static int ident_map_pud(pud_t* pud_page, vir_bytes addr, vir_bytes end,
                         unsigned long offset, unsigned int pmd_flags,
                         int use_gbpages)
{
    vir_bytes next;

    for (; addr < end; addr = next) {
        pud_t* pude = pud_page + ARCH_PUDE(addr);

        next = (addr & ARCH_PUD_MASK) + ARCH_PUD_SIZE;
        if (next > end) next = end;

        if (use_gbpages) {
            pud_t pud_val;

            if (pude_present(*pude)) continue;

            addr &= ARCH_PUD_MASK;
            pud_val = __pud((addr - offset) | pmd_flags);
            *pude = pud_val;
            continue;
        }
    }

    return 0;
}

int pg_identity_map(pde_t* pgd_page, phys_bytes pstart, phys_bytes pend,
                    unsigned long offset)
{
    vir_bytes addr = pstart + offset;
    vir_bytes end = pend + offset;
    vir_bytes next;
    unsigned long pmd_flags =
        ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER | ARCH_PG_BIGPAGE;
    unsigned long page_flags = ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER;
    int use_gbpages;
    int retval;

    use_gbpages = _cpufeature(_CPUF_I386_GBPAGES);

    for (; addr < end; addr = next) {
        pde_t* pgd = pgd_offset(pgd_page, addr);
        pud_t* pud;
        phys_bytes pud_ph;

        next = (addr & ARCH_PGD_MASK) + ARCH_PGD_SIZE;
        if (next > end) next = end;

        if (pde_present(*pgd)) {
            pud = pud_offset(pgd, 0);
            retval =
                ident_map_pud(pud, addr, next, offset, pmd_flags, use_gbpages);
            if (retval) return retval;
            continue;
        }

        pud = pg_alloc_pt(&pud_ph);
        if (!pud) return ENOMEM;

        retval = ident_map_pud(pud, addr, next, offset, pmd_flags, use_gbpages);
        if (retval) return retval;

        *pgd = __pde((pud_ph & ARCH_PG_MASK) | page_flags);
    }

    return 0;
}

#endif

void pg_unmap_identity(pde_t* pgd)
{
    int i;

    for (i = 0; i < ARCH_PDE(VM_STACK_TOP); i++) {
        pde_t pde = pgd[i];

        if (pde_val(pde) & ARCH_PG_BIGPAGE) {
            pgd[i] = __pde(0);
        }
    }
}

void pg_mapkernel(pde_t* pgd)
{
    phys_bytes mapped = 0, kern_phys = kinfo.kernel_start_phys;
    /* phys_bytes kern_len = kinfo.kernel_end_phys - kern_phys; */
    phys_bytes kern_len = LOWMEM_END;
    unsigned long pde = ARCH_PDE(KERNEL_VMA);

    while (mapped < kern_len) {
        pgd[pde] =
            __pde(kern_phys | ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_BIGPAGE);
        mapped += ARCH_BIG_PAGE_SIZE;
        kern_phys += ARCH_BIG_PAGE_SIZE;
        pde++;
    }
}

void pg_load(pde_t* pgd)
{
    write_cr3(__pa(pgd));
    enable_paging();
}

/* <Ring 0> */
void switch_address_space(struct proc* p)
{
    get_cpulocal_var(pt_proc) = p;
    write_cr3(p->seg.cr3_phys);
}

void enable_paging()
{
    u32 cr4;
    int pge_supported;

    pge_supported = _cpufeature(_CPUF_I386_PGE);

    if (pge_supported) {
        cr4 = read_cr4();
        cr4 |= I386_CR4_PGE;
        write_cr4(cr4);
    }
}

/* <Ring 0> */
void disable_paging()
{
    int cr0;
    cr0 = read_cr0();
    cr0 &= ~I386_CR0_PG;
    write_cr0(cr0);
}
