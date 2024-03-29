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
#include <stdint.h>
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/vm.h>
#include <asm/page.h>
#include <lyos/sysutils.h>
#include "proto.h"
#include <asm/pagetable.h>
#include "global.h"
#include "const.h"

#define MAX_KERN_MAPPINGS 10
static struct kern_mapping {
    phys_bytes phys_addr;
    vir_bytes vir_addr;
    size_t len;
    int flags;
} kern_mappings[MAX_KERN_MAPPINGS];
static int nr_kern_mappings = 0;

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];
static struct mm_struct self_mm;

/* before MM has set up page table for its own, we use these pages in page
 * allocation */
static char static_bootstrap_pages[ARCH_PG_SIZE * STATIC_BOOTSTRAP_PAGES]
    __attribute__((aligned(ARCH_PG_SIZE)));

void* phys_to_virt(unsigned long x)
{
    if (!pt_init_done) {
        int i;

        for (i = 0; i < STATIC_BOOTSTRAP_PAGES; i++) {
            if (bootstrap_pages[i].phys_addr == x)
                return bootstrap_pages[i].vir_addr;
        }
    }

    return __va(x);
}

void mm_init(struct mm_struct* mm);

void pt_init()
{
    int i;

    void* bootstrap_pages_mem = static_bootstrap_pages;
    for (i = 0; i < STATIC_BOOTSTRAP_PAGES; i++) {
        void* v = (void*)(bootstrap_pages_mem + i * ARCH_PG_SIZE);
        phys_bytes bootstrap_phys_addr;
        if (umap(SELF, UMT_VADDR, (vir_bytes)v, ARCH_PG_SIZE,
                 &bootstrap_phys_addr))
            panic("MM: can't get phys addr for bootstrap page");
        bootstrap_pages[i].phys_addr = bootstrap_phys_addr;
        bootstrap_pages[i].vir_addr = v;
        bootstrap_pages[i].used = 0;
    }

    /* init mm structure */
    mmprocess->mm = &self_mm;
    mm_init(mmprocess->mm);
    mmprocess->mm->slot = TASK_MM;

    pt_kern_mapping_init();

    /* prepare page directory for MM */
    pgdir_t* mypgd = &mmprocess->mm->pgd;

    if (pgd_new(mypgd)) panic("MM: pgd_new for self failed");

    unsigned long mypdbr = 0;
    static pde_t currentpagedir[ARCH_VM_DIR_ENTRIES];
    if (vmctl_getpdbr(SELF, &mypdbr))
        panic("MM: failed to get page directory base register");
    if (data_copy(SELF, &currentpagedir, NO_TASK, (void*)mypdbr,
                  sizeof(pde_t) * ARCH_VM_DIR_ENTRIES) != 0)
        panic("MM: failed to read initial page directory");

    /* Copy page directory entries for the userspace (MM). */
    for (i = 0; i < VM_STACK_TOP >> ARCH_PGD_SHIFT; i++) {
        pde_t pde = currentpagedir[i];

        if (!pde_present(pde) || pde_bad(pde)) continue;
        mypgd->vir_addr[i] = pde;
    }

    arch_init_pgd(mypgd);

    for (i = 0; i < nr_kern_mappings; i++) {
        arch_create_kern_mapping(kern_mappings[i].phys_addr,
                                 kern_mappings[i].vir_addr,
                                 kern_mappings[i].len, kern_mappings[i].flags);
    }

    /* using the new page dir */
    pgd_bind(mmprocess, mypgd);

    pt_init_done = 1;
}

struct mm_struct* mm_allocate()
{
    struct mm_struct* mm;

    mm = (struct mm_struct*)alloc_vmem(NULL, sizeof(*mm), 0);
    return mm;
}

void mm_init(struct mm_struct* mm)
{
    if (!mm) return;

    memset(mm, 0, sizeof(*mm));

    INIT_LIST_HEAD(&mm->mem_regions);
    region_init_avl(mm);
    INIT_ATOMIC(&mm->refcnt, 1);
}

void mm_free(struct mm_struct* mm)
{
    if (atomic_dec_and_test(&mm->refcnt)) {
        free_vmem(mm, sizeof(*mm));
    }
}

#ifndef __PAGETABLE_PUD_FOLDED
static int __pud_create(pde_t* pde, vir_bytes addr)
{
    phys_bytes pud_phys;
    pud_t* pud = (pud_t*)alloc_vmem(
        &pud_phys, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES, PGT_PAGETABLE);
    if (pud == NULL) {
        printl(
            "MM: pud_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }

    memset(pud, 0, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES);
    free_vmpages(pud, 1);

    pde_populate(pde, pud_phys);

    return 0;
}

static void pud_free(pud_t* pud)
{
    free_mem(virt_to_phys(pud), sizeof(pud_t) * ARCH_VM_PUD_ENTRIES);
}
#else
static inline int __pud_create(pde_t* pde, vir_bytes addr) { return 0; }

static inline void pud_free(pud_t* pud) {}
#endif

pud_t* pud_create(pde_t* pde, vir_bytes addr)
{
    if (pde_none(*pde) && __pud_create(pde, addr)) {
        return NULL;
    }

    return pud_offset(pde, addr);
}

#ifndef __PAGETABLE_PMD_FOLDED
static int __pmd_create(pud_t* pude, vir_bytes addr)
{
    phys_bytes pmd_phys;
    pmd_t* pmd = (pmd_t*)alloc_vmem(
        &pmd_phys, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES, PGT_PAGETABLE);
    if (pmd == NULL) {
        printl(
            "MM: pmd_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }

    memset(pmd, 0, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
    free_vmpages(pmd, 1);

    pude_populate(pude, pmd_phys);

    return 0;
}

static void pmd_free(pmd_t* pmd)
{
    free_mem(virt_to_phys(pmd), sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
}
#else
static inline int __pmd_create(pud_t* pude, vir_bytes addr) { return 0; }

static inline void pmd_free(pmd_t* pmd) {}
#endif

pmd_t* pmd_create(pud_t* pude, vir_bytes addr)
{
    if (pude_none(*pude) && __pmd_create(pude, addr)) {
        return NULL;
    }

    return pmd_offset(pude, addr);
}

static int __pt_create(pmd_t* pmde)
{
    phys_bytes pt_phys;
    pte_t* pt = (pte_t*)alloc_vmem(&pt_phys, sizeof(pte_t) * ARCH_VM_PT_ENTRIES,
                                   PGT_PAGETABLE);
    if (pt == NULL) {
        printl("MM: pt_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }

    memset(pt, 0, sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
    free_vmpages(pt, 1);

    pmde_populate(pmde, pt_phys);

    return 0;
}

pte_t* pt_create_map(pmd_t* pmde, vir_bytes addr)
{
    if (pmde_none(*pmde) && __pt_create(pmde)) {
        return NULL;
    }
    return pte_offset(pmde, addr);
}

static void pt_free(pte_t* pt)
{
    free_mem(virt_to_phys(pt), sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
}

/**
 * <Ring 1> Map a physical page, create page table if necessary.
 * @param  phys_addr Physical address.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
int pt_mappage(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
               pgprot_t prot)
{
    pde_t* pde;
    pud_t* pude;
    pmd_t* pmde;
    pte_t* pte;

    pde = pgd_offset(pgd->vir_addr, vir_addr);
    pude = pud_create(pde, vir_addr);
    pmde = pmd_create(pude, vir_addr);
    pte = pt_create_map(pmde, vir_addr);

    set_pte(pte, pfn_pte(phys_addr >> ARCH_PG_SHIFT, prot));

    return 0;
}

static int pt_follow(pgdir_t* pgd, vir_bytes addr, pte_t** ptepp)
{
    pde_t* pde;
    pud_t* pude;
    pmd_t* pmde;
    pte_t* pte;

    pde = pgd_offset(pgd->vir_addr, addr);
    if (pde_none(*pde)) {
        return EINVAL;
    }

    pude = pud_offset(pde, addr);
    if (pude_none(*pude)) {
        return EINVAL;
    }

    pmde = pmd_offset(pude, addr);
    if (pmde_none(*pmde)) {
        return EINVAL;
    }

    pte = pte_offset(pmde, addr);
    if (!pte_present(*pte)) {
        return EINVAL;
    }

    *ptepp = pte;
    return 0;
}

int pt_writemap(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
                size_t length, pgprot_t prot)
{
    /* sanity check */
    if (phys_addr % ARCH_PG_SIZE != 0) return EINVAL;
    if ((uintptr_t)vir_addr % ARCH_PG_SIZE != 0) return EINVAL;
    if (length % ARCH_PG_SIZE != 0) return EINVAL;

    while (length > 0) {
        pt_mappage(pgd, phys_addr, vir_addr, prot);

        length -= ARCH_PG_SIZE;
        phys_addr = phys_addr + ARCH_PG_SIZE;
        vir_addr = vir_addr + ARCH_PG_SIZE;
    }

    return 0;
}

/**
 * <Ring 1> Initial kernel mappings.
 */
void pt_kern_mapping_init()
{
    int rindex = 0;
    caddr_t addr;
    phys_bytes offset;
    int len, flags;
    struct kern_mapping* kmapping = kern_mappings;
    void* pkmap_start = (void*)PKMAP_START;

    while (!vmctl_get_kern_mapping(rindex, &addr, &len, &flags)) {
        if (rindex > MAX_KERN_MAPPINGS) panic("MM: too many kernel mappings");

        /* fill in mapping information */
        offset = (phys_bytes)addr % ARCH_PG_SIZE;
        kmapping->phys_addr = (phys_bytes)addr - offset;
        kmapping->len = roundup(len + offset, ARCH_PG_SIZE);
        kmapping->flags = flags;

        /* where this region will be mapped */
        kmapping->vir_addr = (vir_bytes)pkmap_start;
        if (!kmapping->vir_addr)
            panic("MM: cannot allocate memory for kernel mappings");

        if (vmctl_reply_kern_mapping(rindex,
                                     (void*)kmapping->vir_addr + offset))
            panic("MM: cannot reply kernel mapping");

        printl("MM: kernel mapping index %d: 0x%016lx - 0x%016lx  (%dkB)\n",
               rindex, kmapping->vir_addr, kmapping->vir_addr + kmapping->len,
               kmapping->len / 1024);

        pkmap_start += kmapping->len;
        nr_kern_mappings++;
        kmapping++;

        rindex++;
    }
}

/* <Ring 1> */
int pgd_new(pgdir_t* pgd)
{
    phys_bytes pgd_phys;
    /* map the directory so that we can write it */
    pde_t* pg_dir = (pde_t*)alloc_vmem(
        &pgd_phys, sizeof(pde_t) * ARCH_VM_DIR_ENTRIES, PGT_PAGEDIR);

    pgd->phys_addr = pgd_phys;
    pgd->vir_addr = pg_dir;

    /* zero it */
    memset(pg_dir, 0, sizeof(pde_t) * ARCH_VM_DIR_ENTRIES);

    arch_pgd_new(pgd);

    return 0;
}

/* <Ring 1> */
int pgd_free(pgdir_t* pgd)
{
    free_vmem(pgd->vir_addr, sizeof(pde_t) * ARCH_VM_DIR_ENTRIES);
    return 0;
}

void pt_free_range(pmd_t* pt)
{
    pte_t* pte = pte_offset(pt, 0);
    pmde_clear(pt);
    pt_free(pte);
}

void pmd_free_range(pud_t* pmd, vir_bytes addr, vir_bytes end, vir_bytes floor,
                    vir_bytes ceiling)
{
    vir_bytes start = addr;
    vir_bytes next;
    pmd_t* pmde = pmd_offset(pmd, addr);

    do {
        next = pmd_addr_end(addr, end);

        if (pmde_none(*pmde) || pmde_bad(*pmde)) {
            pmde++;
            addr = next;
            continue;
        }
        pt_free_range(pmde);

        pmde++;
        addr = next;
    } while (addr != end);

    start &= ARCH_PUD_MASK;
    if (start < floor) return;

    if (ceiling) {
        ceiling &= ARCH_PUD_MASK;
    }

    if (end > ceiling) return;

    pmde = pmd_offset(pmd, 0);
    pude_clear(pmd);
    pmd_free(pmde);
}

void pud_free_range(pde_t* pud, vir_bytes addr, vir_bytes end, vir_bytes floor,
                    vir_bytes ceiling)
{
    vir_bytes start = addr;
    vir_bytes next;
    pud_t* pude = pud_offset(pud, addr);

    do {
        next = pud_addr_end(addr, end);

        if (pude_none(*pude) || pude_bad(*pude)) {
            pude++;
            addr = next;
            continue;
        }
        pmd_free_range(pude, addr, end, floor, ceiling);

        pude++;
        addr = next;
    } while (addr != end);

    start &= ARCH_PGD_MASK;
    if (start < floor) return;

    if (ceiling) {
        ceiling &= ARCH_PGD_MASK;
    }

    if (end > ceiling) return;

    pude = pud_offset(pud, 0);
    pde_clear(pud);
    pud_free(pude);
}

void pgd_free_range(pgdir_t* pgd, vir_bytes addr, vir_bytes end,
                    vir_bytes floor, vir_bytes ceiling)
{
    pde_t* pde = pgd_offset(pgd->vir_addr, addr);
    vir_bytes next;

    do {
        next = pgd_addr_end(addr, end);

        if (pde_none(*pde) || pde_bad(*pde)) {
            pde++;
            addr = next;
            continue;
        }
        pud_free_range(pde, addr, end, floor, ceiling);

        pde++;
        addr = next;
    } while (addr != end);
}

int pgd_bind(struct mmproc* who, pgdir_t* pgd)
{
    /* make sure that the page directory is in low memory */
    return vmctl_set_address_space(who->endpoint, pgd->phys_addr);
}

int pgd_va2pa(pgdir_t* pgd, vir_bytes vir_addr, phys_bytes* phys_addr)
{
    pte_t* pte;
    int retval;

    if ((retval = pt_follow(pgd, (unsigned long)vir_addr, &pte)) != 0) {
        return retval;
    }

    phys_bytes phys = pte_pfn(*pte) << ARCH_PG_SHIFT;
    *phys_addr = phys + ((unsigned long)vir_addr % ARCH_PG_SIZE);
    return 0;
}

int unmap_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length)
{
    /* sanity check */
    if ((uintptr_t)vir_addr % ARCH_PG_SIZE != 0)
        printl("MM: map_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0)
        printl("MM: map_memory: length is not page-aligned!\n");

    while (length > 0) {
        pt_mappage(pgd, 0, vir_addr, __pgprot(0));

        length -= ARCH_PG_SIZE;
        vir_addr += ARCH_PG_SIZE;
    }

    return 0;
}
