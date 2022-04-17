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
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/vm.h>
#include "region.h"
#include "proto.h"
#include <lyos/cpufeature.h>
#include <asm/pagetable.h>
#include "global.h"
#include "const.h"

unsigned long va_pa_offset;

static pgdir_t kern_pg_dir;

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];

static pmd_t* alloc_pmd(phys_bytes* pmd_phys)
{
    pmd_t* pmd = (pmd_t*)alloc_vmem(
        pmd_phys, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES, PGT_PAGETABLE);
    if (!pmd) return NULL;

    memset(pmd, 0, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
    return pmd;
}

static pud_t* alloc_pud(phys_bytes* pud_phys)
{
    pud_t* pud = (pud_t*)alloc_vmem(
        pud_phys, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES, PGT_PAGETABLE);
    if (!pud) return NULL;

    memset(pud, 0, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES);
    return pud;
}

static void setup_kern_pgd(pgdir_t* pgd)
{
    pde_t* kern_pgd;
    phys_bytes kern_pgd_phys;
    unsigned long kpdbr;

    if (vmctl_getkpdbr(SELF, &kpdbr))
        panic("MM: failed to get kernel page directory base register");

    kern_pgd = (pde_t*)alloc_vmem(
        &kern_pgd_phys, sizeof(pde_t) * ARCH_VM_DIR_ENTRIES, PGT_PAGETABLE);
    if (!kern_pgd) panic("MM: cannot allocate kernel page directory");

    if (data_copy(SELF, kern_pgd, NO_TASK, (void*)kpdbr,
                  sizeof(pde_t) * ARCH_VM_DIR_ENTRIES) != 0)
        panic("MM: failed to read kernel page directory");

    if (vmctl_set_kern_address_space(kern_pgd_phys))
        panic("MM: failed to set kernel page directory base register");

    pgd->phys_addr = kern_pgd_phys;
    pgd->vir_addr = kern_pgd;
}

static void alloc_init_pmd(pud_t* pude, unsigned long addr, unsigned long end,
                           phys_bytes phys, pgprot_t prot)
{
    unsigned long next;
    pmd_t* pmde;

    if (pude_none(*pude)) {
        phys_bytes pmd_phys;

        alloc_pmd(&pmd_phys);
        pude_populate(pude, pmd_phys);
    }

    pmde = pmd_offset(pude, addr);
    do {
        next = pmd_addr_end(addr, end);

        if (((addr | next | phys) & ~ARCH_PMD_MASK) == 0) {
            set_pmde(pmde,
                     pfn_pmd(phys >> ARCH_PG_SHIFT, mk_pmd_sect_prot(prot)));
        } else {
        }
        phys += next - addr;
    } while (pmde++, addr = next, addr != end);
}

static void alloc_init_pud(pde_t* pde, unsigned long addr, unsigned long end,
                           phys_bytes phys, pgprot_t prot)
{
    unsigned long next;
    pud_t* pude;

    if (pde_none(*pde)) {
        phys_bytes pud_phys;

        alloc_pud(&pud_phys);
        pde_populate(pde, pud_phys);
    }

    pude = pud_offset(pde, addr);
    do {
        next = pud_addr_end(addr, end);

        if (pud_sect_supported() &&
            ((addr | next | phys) & ~ARCH_PUD_MASK) == 0) {
            set_pude(pude,
                     pfn_pud(phys >> ARCH_PG_SHIFT, mk_pud_sect_prot(prot)));
        } else {
            alloc_init_pmd(pude, addr, next, phys, prot);
        }

        phys += next - addr;
    } while (pude++, addr = next, addr != end);
}

static void __create_pgd_mapping(pgdir_t* pgd, phys_bytes phys, vir_bytes virt,
                                 phys_bytes size, pgprot_t prot)
{
    unsigned long addr, end, next;
    pde_t* pde = pgd_offset(pgd->vir_addr, virt);

    phys &= ARCH_PG_MASK;
    addr = virt & ARCH_PG_MASK;
    end = roundup(virt + size, ARCH_PG_SIZE);

    do {
        next = pgd_addr_end(addr, end);
        alloc_init_pud(pde, addr, next, phys, prot);
        phys += next - addr;
    } while (pde++, addr = next, addr != end);
}

void arch_init_pgd(pgdir_t* pgd)
{
    int i;
    struct kinfo_mmap_entry* mmap;
    pgdir_t* mypgd = &mmprocess->mm->pgd;

    setup_kern_pgd(&kern_pg_dir);

    va_pa_offset = MM_PAGE_OFFSET;

    for (i = 0, mmap = kernel_info.memmaps; i < kernel_info.memmaps_count;
         i++, mmap++) {
        phys_bytes start = mmap->addr;
        phys_bytes end = mmap->addr + mmap->len;

        start &= ARCH_PUD_MASK;
        end = roundup(end, ARCH_PUD_SIZE);

        if (start >= end) break;

        __create_pgd_mapping(mypgd, start, __phys_to_virt(start), end - start,
                             ARM64_PG_SHARED);
    }
}

void arch_create_kern_mapping(phys_bytes phys_addr, vir_bytes vir_addr,
                              size_t len, int flags)
{
    unsigned long page_prot;

    if (flags & KMF_IO)
        page_prot = _ARM64_DEVICE_nGnRE;
    else
        page_prot = _ARM64_PG_NORMAL;

    if (flags & KMF_USER) page_prot |= _ARM64_PTE_USER;

    if (!(flags & KMF_WRITE)) page_prot |= _ARM64_PTE_RDONLY;

    if (flags & KMF_EXEC) {
        if (flags & KMF_USER)
            page_prot &= ~_ARM64_PTE_UXN;
        else
            page_prot &= ~_ARM64_PTE_PXN;
    }

    pt_writemap(&kern_pg_dir, phys_addr, vir_addr, len, __pgprot(page_prot));
}

void arch_pgd_new(pgdir_t* pgd) {}
