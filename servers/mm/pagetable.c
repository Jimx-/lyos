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
#include "lyos/config.h"
#include "stdio.h"
#include <stdint.h>
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/vm.h>
#include "region.h"
#include "proto.h"
#include <lyos/cpufeature.h>
#include "global.h"
#include "const.h"
#include "pagetable.h"

#define MAX_KERN_MAPPINGS 10
static struct kern_mapping {
    phys_bytes phys_addr;
    vir_bytes vir_addr;
    size_t len;
    int flags;
} kern_mappings[MAX_KERN_MAPPINGS];
static int nr_kern_mappings = 0;

#if defined(__i386__)
static int global_bit = 0;
#endif

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];
static struct mm_struct self_mm;
//#define PAGETABLE_DEBUG    1

/* before MM has set up page table for its own, we use these pages in page
 * allocation */
static char static_bootstrap_pages[ARCH_PG_SIZE * STATIC_BOOTSTRAP_PAGES]
    __attribute__((aligned(ARCH_PG_SIZE)));

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

#if defined(__i386__)
    if (_cpufeature(_CPUF_I386_PGE)) global_bit = ARCH_PG_GLOBAL;
#endif

    /* init mm structure */
    mmprocess->mm = &self_mm;
    mm_init(mmprocess->mm);
    mmprocess->mm->slot = TASK_MM;

    pt_kern_mapping_init();

    /* prepare page directory for MM */
    pgdir_t* mypgd = &mmprocess->mm->pgd;

    if (pgd_new(mypgd)) panic("MM: pgd_new for self failed");

    int kernel_pde = kernel_info.kernel_start_pde;
    phys_bytes paddr = kernel_info.kernel_start_phys;

    /* map kernel for MM */
    while (kernel_pde < ARCH_PDE(KERNEL_VMA + LOWMEM_END)) {
#if defined(__i386__)
        mypgd->vir_addr[kernel_pde] =
            __pde(paddr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW);
#elif defined(__arm__)
        mypgd->vir_addr[kernel_pde] =
            __pde((paddr & ARM_VM_SECTION_MASK) | ARM_VM_SECTION |
                  ARM_VM_SECTION_DOMAIN | ARM_VM_SECTION_CACHED |
                  ARM_VM_SECTION_SUPER);
#endif

        paddr += ARCH_BIG_PAGE_SIZE;
        kernel_pde++;
    }

    /* create direct mapping to access physical memory */
    for (kernel_pde = 0, paddr = 0; kernel_pde < ARCH_PDE(LOWMEM_END);
         kernel_pde++, paddr += ARCH_BIG_PAGE_SIZE) {
        if (paddr < kernel_info.kernel_end_phys) {
            continue;
        }

#if defined(__i386__)
        mypgd->vir_addr[kernel_pde] =
            __pde(paddr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW |
                  ARCH_PG_USER);
#elif defined(__arm__)
        mypgd->vir_addr[kernel_pde] =
            __pde((paddr & ARM_VM_SECTION_MASK) | ARM_VM_SECTION |
                  ARM_VM_SECTION_DOMAIN | ARM_VM_SECTION_CACHED |
                  ARM_VM_SECTION_SUPER);
#endif
    }

    unsigned int mypdbr = 0;
    static pde_t currentpagedir[ARCH_VM_DIR_ENTRIES];
    if (vmctl_getpdbr(SELF, &mypdbr))
        panic("MM: failed to get page directory base register");
    /* kernel has done identity mapping for the bootstrap page dir we are using,
     * so this is ok */
    data_copy(SELF, &currentpagedir, NO_TASK, (void*)mypdbr,
              sizeof(pde_t) * ARCH_VM_DIR_ENTRIES);

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pde_t entry = currentpagedir[i];

        if (!(pde_val(entry) & ARCH_PG_PRESENT)) continue;
        if (pde_val(entry) & ARCH_PG_BIGPAGE) {
            continue;
        }

        /* if (pt_create((pmd_t*)&mypgd->vir_addr[i]) != 0) { */
        /*     panic("MM: failed to allocate page table for MM"); */
        /* } */

        /* phys_bytes ptphys_kern = pde_val(entry) & ARCH_PG_MASK; */
        /* phys_bytes ptphys_mm = pde_val(mypgd->vir_addr[i]) & ARCH_PG_MASK; */
        /* data_copy(NO_TASK, (void*)ptphys_mm, NO_TASK, (void*)ptphys_kern,
         * ARCH_PG_SIZE); */
        mypgd->vir_addr[i] = entry;
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

#ifndef __PAGETABLE_PMD_FOLDED
static void __pmd_create(pde_t* pde, vir_bytes addr) {}
#else
#define __pmd_create(pde, addr) NULL
#endif

pmd_t* pmd_create(pde_t* pde, vir_bytes addr)
{
    if (pde_none(*pde)) {
        __pmd_create(pde, addr);
    }

    return pmd_offset(pde, addr);
}

int pt_create(pmd_t* pmde)
{
    if (!pmde_none(*pmde)) {
        /* page table already created */
        return 0;
    }

    phys_bytes pt_phys;
    pte_t* pt = (pte_t*)alloc_vmem(&pt_phys, sizeof(pte_t) * ARCH_VM_PT_ENTRIES,
                                   PGT_PAGETABLE);
    if (pt == NULL) {
        printl("MM: pt_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }

#if PAGETABLE_DEBUG
    printl("MM: pt_create: allocated new page table\n");
#endif

    int i;
    for (i = 0; i < ARCH_VM_PT_ENTRIES; i++) {
        pt[i] = __pte(0);
    }

#ifdef __i386__
    pmde_populate(pmde, pt_phys);
#elif defined(__arm__)
    pgd->vir_addr[pde] = __pde((pt_phys & ARM_VM_PDE_MASK) |
                               ARM_VM_PDE_PRESENT | ARM_VM_PDE_DOMAIN);
#endif

    return 0;
}

pte_t* pt_create_map(pmd_t* pmde, vir_bytes addr)
{
    pt_create(pmde);
    return pte_offset(pmde, addr);
}

/**
 * <Ring 1> Map a physical page, create page table if necessary.
 * @param  phys_addr Physical address.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
int pt_mappage(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
               unsigned int flags)
{
    pde_t* pde;
    pmd_t* pmde;
    pte_t* pte;

    pde = pgd_offset(pgd->vir_addr, vir_addr);
    pmde = pmd_create(pde, vir_addr);
    pte = pt_create_map(pmde, vir_addr);

    *pte = __pte((phys_addr & ARCH_PG_MASK) | flags);

    return 0;
}

static int pt_follow(pgdir_t* pgd, vir_bytes addr, pte_t** ptepp)
{
    pde_t* pde;
    pmd_t* pmde;
    pte_t* pte;

    pde = pgd_offset(pgd->vir_addr, addr);
    if (pde_none(*pde)) {
        return EINVAL;
    }

    pmde = pmd_offset(pde, addr);
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

/**
 * <Ring 1> Make a physical page write-protected.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
int pt_wppage(pgdir_t* pgd, vir_bytes vir_addr)
{
    pte_t* pte;
    int retval;

    if ((retval = pt_follow(pgd, vir_addr, &pte)) != 0) {
        return retval;
    }

    *pte = __pte(pte_val(*pte) & (~ARCH_PG_RW));

    return 0;
}

/**
 * <Ring 1> Make a physical page read-write.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
int pt_unwppage(pgdir_t* pgd, vir_bytes vir_addr)
{
    pte_t* pte;
    int retval;

    if ((retval = pt_follow(pgd, vir_addr, &pte)) != 0) {
        return retval;
    }
    *pte = __pte(pte_val(*pte) | ARCH_PG_RW);

    return 0;
}

int pt_writemap(pgdir_t* pgd, phys_bytes phys_addr, vir_bytes vir_addr,
                size_t length, int flags)
{
    /* sanity check */
    if (phys_addr % ARCH_PG_SIZE != 0) return EINVAL;
    if ((uintptr_t)vir_addr % ARCH_PG_SIZE != 0) return EINVAL;
    if (length % ARCH_PG_SIZE != 0) return EINVAL;

    while (length > 0) {
        pt_mappage(pgd, phys_addr, vir_addr, flags);

        length -= ARCH_PG_SIZE;
        phys_addr = phys_addr + ARCH_PG_SIZE;
        vir_addr = vir_addr + ARCH_PG_SIZE;
    }

    return 0;
}

int pt_wp_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length)
{
    /* sanity check */
    if (vir_addr % ARCH_PG_SIZE != 0)
        printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0)
        printl("MM: pt_wp_memory: length is not page-aligned!\n");

    while (1) {
        pt_wppage(pgd, vir_addr);

        length -= ARCH_PG_SIZE;
        vir_addr += ARCH_PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

int pt_unwp_memory(pgdir_t* pgd, vir_bytes vir_addr, size_t length)
{
    /* sanity check */
    if (vir_addr % ARCH_PG_SIZE != 0)
        printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0)
        printl("MM: pt_wp_memory: length is not page-aligned!\n");

    while (1) {
        pt_unwppage(pgd, vir_addr);

        length -= ARCH_PG_SIZE;
        vir_addr += ARCH_PG_SIZE;
        if (length <= 0) break;
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
    int len, flags;
    struct kern_mapping* kmapping = kern_mappings;
    void* pkmap_start = (void*)PKMAP_START;

    while (!vmctl_get_kern_mapping(rindex, &addr, &len, &flags)) {
        if (rindex > MAX_KERN_MAPPINGS) panic("MM: too many kernel mappings");

        /* fill in mapping information */
        kmapping->phys_addr = (phys_bytes)addr;
        kmapping->len = len;

        kmapping->flags = ARCH_PG_PRESENT;
        if (flags & KMF_USER) kmapping->flags |= ARCH_PG_USER;
#if defined(__arm__)
        else
            kmapping->flags |= ARM_PG_SUPER;
#endif
        if (flags & KMF_WRITE)
            kmapping->flags |= ARCH_PG_RW;
        else
            kmapping->flags |= ARCH_PG_RO;

#if defined(__arm__)
        kmapping->flags |= ARM_PG_CACHED;
#endif

        /* where this region will be mapped */
        kmapping->vir_addr = (vir_bytes)pkmap_start;
        if (!kmapping->vir_addr)
            panic("MM: cannot allocate memory for kernel mappings");

        if (vmctl_reply_kern_mapping(rindex, (void*)kmapping->vir_addr))
            panic("MM: cannot reply kernel mapping");

        printl("MM: kernel mapping index %d: 0x%08x - 0x%08x  (%dkB)\n", rindex,
               kmapping->vir_addr, (int)kmapping->vir_addr + kmapping->len,
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

    pgd->phys_addr = (void*)pgd_phys;
    pgd->vir_addr = pg_dir;

    int i;

    /* zero it */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pg_dir[i] = __pde(0);
    }

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pgd->vir_pts[i] = NULL;
    }

    pgd_mapkernel(pgd);
    return 0;
}

/**
 * <Ring 1> Map the kernel.
 * @param  pgd The page directory.
 * @return     Zero on success.
 */
int pgd_mapkernel(pgdir_t* pgd)
{
    int i;
    int kernel_pde = kernel_info.kernel_start_pde;
    phys_bytes addr = kernel_info.kernel_start_phys;

    /* map low memory */
    while (kernel_pde < ARCH_PDE(KERNEL_VMA + LOWMEM_END)) {
#if defined(__i386__)
        pgd->vir_addr[kernel_pde] =
            __pde(addr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW);
#elif defined(__arm__)
        pgd->vir_addr[kernel_pde] =
            __pde((addr & ARM_VM_SECTION_MASK) | ARM_VM_SECTION |
                  ARM_VM_SECTION_DOMAIN | ARM_VM_SECTION_CACHED |
                  ARM_VM_SECTION_SUPER);
#endif

        addr += ARCH_BIG_PAGE_SIZE;
        kernel_pde++;
    }

    for (i = 0; i < nr_kern_mappings; i++) {
        pt_writemap(pgd, kern_mappings[i].phys_addr, kern_mappings[i].vir_addr,
                    kern_mappings[i].len, kern_mappings[i].flags);
    }

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
    free_vmem(pte, sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
}

void pmd_free_range(pde_t* pmd, vir_bytes addr, vir_bytes end, vir_bytes floor,
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

    start &= ARCH_PGD_MASK;
    if (start < floor) return;

    if (ceiling) {
        ceiling &= ARCH_PGD_MASK;
    }

    if (end > ceiling) return;

    pmde = pmd_offset(pmd, 0);
    pde_clear(pmd);
    free_vmem(pmde, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
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
        pmd_free_range(pde, addr, end, floor, ceiling);

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

    if (vir_addr >= KERNEL_VMA) {
        *phys_addr = __pa(vir_addr);
        return 0;
    }

    if ((retval = pt_follow(pgd, (unsigned long)vir_addr, &pte)) != 0) {
        return retval;
    }

    phys_bytes phys = pte_val(*pte) & ARCH_PG_MASK;
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
        pt_mappage(pgd, 0, vir_addr, 0);

        length -= ARCH_PG_SIZE;
        vir_addr += ARCH_PG_SIZE;
    }

    return 0;
}
