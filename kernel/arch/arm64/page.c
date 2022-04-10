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
#include <asm/fixmap.h>

#include <libfdt/libfdt.h>

static pte_t bm_pt[ARCH_VM_PT_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));
static pmd_t bm_pmd[ARCH_VM_PMD_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));
static pud_t bm_pud[ARCH_VM_PUD_ENTRIES] __attribute__((aligned(ARCH_PG_SIZE)));

static inline pud_t* fixmap_pude(unsigned long addr)
{
    pde_t* pde;

    pde = pgd_offset(init_pg_dir, addr);
    return pud_offset_kimg(pde, addr);
}

static inline pmd_t* fixmap_pmde(unsigned long addr)
{
    pud_t* pude = fixmap_pude(addr);
    return pmd_offset_kimg(pude, addr);
}

void early_fixmap_init(void)
{
    pde_t* pde;
    pud_t* pude;
    pmd_t* pmde;
    vir_bytes addr = FIXADDR_START;

    pde = pgd_offset(init_pg_dir, addr);
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
    const phys_bytes dt_phys_base = rounddown(dt_phys, SWAPPER_BLOCK_SIZE);
    int offset;
    void* dt_virt;
    pmd_t* pmde;

    if (!dt_phys || dt_phys % 8) return NULL;

    offset = dt_phys % SWAPPER_BLOCK_SIZE;
    dt_virt = (void*)dt_virt_base + offset;

    pmde = fixmap_pmde(dt_virt_base);
    *pmde = pfn_pmd(dt_phys_base >> ARCH_PG_SHIFT, mk_pmd_sect_prot(prot));

    if (fdt_magic(dt_virt) != FDT_MAGIC) return NULL;

    *size = fdt_totalsize(dt_virt);
    if (*size > (2 << 20)) return NULL;

    if (offset + *size > SWAPPER_BLOCK_SIZE) {
        pmde = fixmap_pmde(dt_virt_base + SWAPPER_BLOCK_SIZE);
        *pmde = pfn_pmd((dt_phys_base + SWAPPER_BLOCK_SIZE) >> ARCH_PG_SHIFT,
                        mk_pmd_sect_prot(prot));
    }

    return dt_virt;
}

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

void switch_address_space(struct proc* p) {}
