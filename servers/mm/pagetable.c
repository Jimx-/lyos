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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
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

#define MAX_KERN_MAPPINGS   10
PRIVATE struct kern_mapping {
    void * phys_addr;
    void * vir_addr;
    int len;
    int flags;
} kern_mappings[MAX_KERN_MAPPINGS];
PRIVATE int nr_kern_mappings = 0;

#if defined(__i386__)
PRIVATE int global_bit = 0;
#endif

PRIVATE struct pagedir_mapping {
    int pde_no;
    phys_bytes phys_addr;
    pte_t * entries;
    pde_t val;
} pagedir_mappings[MAX_PAGEDIR_PDES];

PRIVATE struct mmproc * mmprocess = &mmproc_table[TASK_MM];
PRIVATE struct mm_struct self_mm;
//#define PAGETABLE_DEBUG    1

/* before MM has set up page table for its own, we use these pages in page allocation */
PRIVATE char static_bootstrap_pages[ARCH_PG_SIZE * STATIC_BOOTSTRAP_PAGES] 
        __attribute__((aligned(ARCH_PG_SIZE)));

PUBLIC void pt_init()
{
    int i;

    vir_bytes bootstrap_pages_mem = (vir_bytes)static_bootstrap_pages;
    for (i = 0; i < STATIC_BOOTSTRAP_PAGES; i++) {
        void * v = (void *)(bootstrap_pages_mem + i * ARCH_PG_SIZE);
        phys_bytes bootstrap_phys_addr;
        if (umap(SELF, v, &bootstrap_phys_addr)) panic("MM: can't get phys addr for bootstrap page");
        bootstrap_pages[i].phys_addr = bootstrap_phys_addr;
        bootstrap_pages[i].vir_addr = (vir_bytes)v;
        bootstrap_pages[i].used = 0;
    }

#if defined(__i386__)
    if (_cpufeature(_CPUF_I386_PGE))
        global_bit = ARCH_PG_GLOBAL;
#endif

    /* init mm structure */
    mmprocess->mm = &self_mm;
    mm_init(mmprocess->mm);
    mmprocess->mm->slot = TASK_MM;
    mmprocess->active_mm = mmprocess->mm;

    /* Setting up page dir mappings, which make all page dirs visible to kernel */
    int end_pde = kernel_info.kernel_end_pde;
    for (i = 0; i < MAX_PAGEDIR_PDES; i++) {
        struct pagedir_mapping * pdm = &pagedir_mappings[i];
        pdm->pde_no = end_pde++;
        phys_bytes phys_addr;

        if ((pdm->entries = (pte_t *)alloc_vmem(&phys_addr, ARCH_PG_SIZE, PGT_PAGETABLE)) == NULL) 
            panic("MM: no memory for pagedir mappings");
        
        pdm->phys_addr = phys_addr;
#if defined(__i386__)
        pdm->val = (phys_addr & ARCH_VM_ADDR_MASK) | ARCH_PG_PRESENT | ARCH_PG_RW;
#elif defined(__arm__)
        pdm->val = (phys_addr & ARCH_VM_ADDR_MASK) | ARM_VM_PDE_PRESENT | ARM_PG_CACHED | ARM_VM_PDE_DOMAIN;
#endif
    }

    pt_kern_mapping_init();

    /* prepare page directory for MM */
    pgdir_t * mypgd = &mmprocess->mm->pgd;

    if (pgd_new(mypgd)) panic("MM: pgd_new for self failed");
    
    unsigned int mypdbr = 0;
    static pde_t currentpagedir[ARCH_VM_DIR_ENTRIES];
    if (vmctl_getpdbr(SELF, &mypdbr)) panic("MM: failed to get page directory base register");
    /* kernel has done identity mapping for the bootstrap page dir we are using, so this is ok */
    data_copy(SELF, &currentpagedir, NO_TASK, (void *)mypdbr, ARCH_PGD_SIZE); 

    for(i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pde_t entry = currentpagedir[i];

        if (entry & ARCH_PG_BIGPAGE) continue;
        mypgd->vir_addr[i] = entry;
        mypgd->vir_pts[i] = (pte_t *)((entry + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
    }

    /* using the new page dir */
    pgd_bind(mmprocess, mypgd);

    pt_init_done = 1;
}

PUBLIC struct mm_struct* mm_allocate()
{
    struct mm_struct* mm;

    int len = roundup(sizeof(struct mm_struct), ARCH_PG_SIZE);
    mm = alloc_vmem(NULL, len, 0);
    return mm;
}

PUBLIC void mm_init(struct mm_struct* mm)
{
    if (!mm) return;

    INIT_LIST_HEAD(&mm->mem_regions);
    region_init_avl(mm);
    INIT_ATOMIC(&mm->refcnt, 1);
}

PUBLIC void mm_free(struct mm_struct* mm)
{
    if (atomic_dec_and_test(&mm->refcnt)) {
        int len = roundup(sizeof(struct mm_struct), ARCH_PG_SIZE);
        free_vmem(mm, len);
    }
}

PUBLIC int pt_create(pgdir_t * pgd, int pde, u32 flags)
{
    phys_bytes pt_phys;
    pte_t * pt = (pte_t *)alloc_vmem(&pt_phys, ARCH_PT_SIZE, PGT_PAGETABLE);
    if (pt == NULL) {
        printl("MM: pt_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }
    if (pgd->vir_pts[pde]) {
        free_vmem(pt, ARCH_PT_SIZE);
        return 0;
    }

#if PAGETABLE_DEBUG
        printl("MM: pt_create: allocated new page table\n");
#endif

    int i;
    for (i = 0; i < ARCH_VM_PT_ENTRIES; i++) {
        pt[i] = 0;
    }

#ifdef __i386__
    pgd->vir_addr[pde] = pt_phys | flags;
#elif defined(__arm__)
    pgd->vir_addr[pde] = (pt_phys & ARM_VM_PDE_MASK) | ARM_VM_PDE_PRESENT | ARM_VM_PDE_DOMAIN;
#endif
    pgd->vir_pts[pde] = pt;

    return 0;
}

/**
 * <Ring 1> Map a physical page, create page table if necessary.
 * @param  phys_addr Physical address.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_mappage(pgdir_t * pgd, void * phys_addr, void * vir_addr, u32 flags)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];

    /* page table not present */
    if (pt == NULL) {
        int retval = pt_create(pgd, pgd_index, ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER);

        if (retval) return retval;
        pt = pgd->vir_pts[pgd_index];
    }

    pt[pt_index] = ((int)phys_addr & 0xFFFFF000) | flags;

    return 0;
}

/**
 * <Ring 1> Make a physical page write-protected.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_wppage(pgdir_t * pgd, void * vir_addr)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];
    if (pt) pt[pt_index] &= ~ARCH_PG_RW;

    return 0;
}

/**
 * <Ring 1> Make a physical page read-write.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_unwppage(pgdir_t * pgd, void * vir_addr)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];
    if (pt) pt[pt_index] |= ARCH_PG_RW;

    return 0;
}

PUBLIC int pt_writemap(pgdir_t * pgd, void * phys_addr, void * vir_addr, int length, int flags)
{
    /* sanity check */
    if ((int)phys_addr % ARCH_PG_SIZE != 0) printl("MM: pt_writemap: phys_addr is not page-aligned!\n");
    if ((int)vir_addr % ARCH_PG_SIZE != 0) printl("MM: pt_writemap: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0) printl("MM: pt_writemap: length is not page-aligned!\n");
    
    while (1) {
        pt_mappage(pgd, phys_addr, vir_addr, flags);

        length -= ARCH_PG_SIZE;
        phys_addr = (void *)((int)phys_addr + ARCH_PG_SIZE);
        vir_addr = (void *)((int)vir_addr + ARCH_PG_SIZE);
        if (length <= 0) break;
    }

    return 0;
}

PUBLIC int pt_wp_memory(pgdir_t * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((vir_bytes)vir_addr % ARCH_PG_SIZE != 0) printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0) printl("MM: pt_wp_memory: length is not page-aligned!\n");

    while (1) {
        pt_wppage(pgd, vir_addr);

        length -= ARCH_PG_SIZE;
        vir_addr = (void *)((vir_bytes)vir_addr + ARCH_PG_SIZE);
        if (length <= 0) break;
    }

    return 0;
}

PUBLIC int pt_unwp_memory(pgdir_t * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)vir_addr % ARCH_PG_SIZE != 0) printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0) printl("MM: pt_wp_memory: length is not page-aligned!\n");
    
    while (1) {
        pt_unwppage(pgd, vir_addr);

        length -= ARCH_PG_SIZE;
        vir_addr = (void *)((int)vir_addr + ARCH_PG_SIZE);
        if (length <= 0) break;
    }

    return 0;
}

/**
 * <Ring 1> Initial kernel mappings.
 */
PUBLIC void pt_kern_mapping_init()
{
    int rindex = 0;
    caddr_t addr;
    int len, flags;
    struct kern_mapping * kmapping = kern_mappings;

    while (!vmctl_get_kern_mapping(rindex, &addr, &len, &flags)) {
        if (rindex > MAX_KERN_MAPPINGS) panic("MM: too many kernel mappings");

        /* fill in mapping information */
        kmapping->phys_addr = addr;
        kmapping->len = len;

        kmapping->flags = ARCH_PG_PRESENT;
        if (flags & KMF_USER) kmapping->flags |= ARCH_PG_USER;
#if defined(__arm__)
        else kmapping->flags |= ARM_PG_SUPER;
#endif
        if (flags & KMF_WRITE) kmapping->flags |= ARCH_PG_RW;
        else kmapping->flags |= ARCH_PG_RO;

#if defined(__arm__)
        kmapping->flags |= ARM_PG_CACHED;
#endif

        /* where this region will be mapped */
        kmapping->vir_addr = (void *)alloc_vmpages(kmapping->len / ARCH_PG_SIZE);
        if (kmapping->vir_addr == NULL) panic("MM: cannot allocate memory for kernel mappings");

        if (vmctl_reply_kern_mapping(rindex, kmapping->vir_addr)) panic("MM: cannot reply kernel mapping");

        printl("MM: kernel mapping index %d: 0x%08x - 0x%08x  (%dkB)\n", 
                rindex, kmapping->vir_addr, (int)kmapping->vir_addr + kmapping->len, kmapping->len / 1024);

        nr_kern_mappings++;
        kmapping++;
        rindex++;
    }
}

/* <Ring 1> */
PUBLIC int pgd_new(pgdir_t * pgd)
{
    phys_bytes pgd_phys;
    /* map the directory so that we can write it */
    pde_t * pg_dir = (pde_t *)alloc_vmem(&pgd_phys, ARCH_PGD_SIZE, PGT_PAGEDIR);

    pgd->phys_addr = (void *)pgd_phys;
    pgd->vir_addr = pg_dir;

    int i;

    /* zero it */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pg_dir[i] = 0;
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
PUBLIC int pgd_mapkernel(pgdir_t * pgd)
{
    int i;
    int kernel_pde = kernel_info.kernel_start_pde;
    unsigned int addr = kernel_info.kernel_start_phys, mapped = 0, kern_size = (kernel_info.kernel_end_pde - kernel_info.kernel_start_pde) * ARCH_BIG_PAGE_SIZE;

    while (mapped < kern_size) {
#if defined(__i386__)
        pgd->vir_addr[kernel_pde] = addr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW | global_bit;
#elif defined(__arm__)
        pgd->vir_addr[kernel_pde] = (addr & ARM_VM_SECTION_MASK)
            | ARM_VM_SECTION
            | ARM_VM_SECTION_DOMAIN
            | ARM_VM_SECTION_CACHED
            | ARM_VM_SECTION_SUPER;
#endif

        addr += ARCH_BIG_PAGE_SIZE;
        mapped += ARCH_BIG_PAGE_SIZE;
        kernel_pde++;
    }

    /* map page dirs */
    for(i = 0; i < MAX_PAGEDIR_PDES; i++) {
        struct pagedir_mapping * pdm = &pagedir_mappings[i];
        pgd->vir_addr[pdm->pde_no] = pdm->val;
    }

    for (i = 0; i < nr_kern_mappings; i++) {
        pt_writemap(pgd, kern_mappings[i].phys_addr, kern_mappings[i].vir_addr, kern_mappings[i].len, kern_mappings[i].flags);
    }

    return 0;
}

/* <Ring 1> */
PUBLIC int pgd_free(pgdir_t * pgd)
{
    pgd_clear(pgd);
    free_vmem((vir_bytes) pgd->vir_addr, ARCH_PGD_SIZE);
    
    return 0;
}

PUBLIC int pgd_clear(pgdir_t * pgd)
{
    int i;

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        if (pgd->vir_pts[i]) {
            free_vmem((vir_bytes)pgd->vir_pts[i], ARCH_PT_SIZE);
        }
        pgd->vir_pts[i] = NULL;
    }
    
    return 0;
}

PUBLIC int pgd_bind(struct mmproc * who, pgdir_t * pgd)
{
    int slot = who->active_mm->slot, pdm_slot;
    int pages_per_pgdir = ARCH_PGD_SIZE / ARCH_PG_SIZE;
    int slots_per_pdm = ARCH_VM_PT_ENTRIES / pages_per_pgdir;

    /* fill in the slot */
    struct pagedir_mapping * pdm = &pagedir_mappings[slot / slots_per_pdm];
    pdm_slot = slot % slots_per_pdm;

    phys_bytes phys = (phys_bytes)pgd->phys_addr & ARCH_VM_ADDR_MASK;
#if defined(__i386__)
    pdm->entries[pdm_slot] = phys | ARCH_PG_PRESENT | ARCH_PG_RW;
#elif defined(__arm__)
    int _i;
    for (_i = 0; _i < pages_per_pgdir; _i++) {
        pdm->entries[pdm_slot * pages_per_pgdir + _i] = (phys + _i * ARCH_PG_SIZE) | ARCH_PG_PRESENT
            | ARCH_PG_RW
            | ARM_PG_CACHED
            | ARCH_PG_USER;
    }
#endif

#if defined(__i386__)
    /* calculate the vir addr of the pgdir visible to the kernel */
    vir_bytes vir_addr = pdm->pde_no * ARCH_BIG_PAGE_SIZE + pdm_slot * ARCH_PG_SIZE;
#elif defined(__arm__)
    vir_bytes vir_addr = pdm->pde_no * ARCH_BIG_PAGE_SIZE + pdm_slot * ARCH_PGD_SIZE;
#endif

    return vmctl_set_address_space(who->endpoint, pgd->phys_addr, (void *)vir_addr);
}

PUBLIC vir_bytes pgd_find_free_pages(pgdir_t * pgd, int nr_pages, vir_bytes minv, vir_bytes maxv)
{
    unsigned int start_pde = ARCH_PDE(minv);
    unsigned int end_pde = ARCH_PDE(maxv);

    int i, j;
    int allocated_pages = 0;
    vir_bytes retaddr = 0;
    for (i = start_pde; i < end_pde; i++) {
        pte_t * pt_entries = pgd->vir_pts[i];
        /* the pde is empty, we have I386_VM_DIR_ENTRIES free pages */
        if (pt_entries == NULL) {
            nr_pages -= ARCH_VM_PT_ENTRIES;
            allocated_pages += ARCH_VM_PT_ENTRIES;
            if (retaddr == 0) retaddr = ARCH_VM_ADDRESS(i, 0, 0);
            if (nr_pages <= 0) {
                return retaddr;
            }
            continue;
        }

        for (j = 0; j < ARCH_VM_PT_ENTRIES; j++) {
            if (pt_entries[j] != 0) {
                nr_pages += allocated_pages;
                retaddr = 0;
                allocated_pages = 0;
            } else {
                nr_pages--;
                allocated_pages++;
                if (!retaddr) retaddr = ARCH_VM_ADDRESS(i, j, 0);
            }

            if (nr_pages <= 0) return retaddr;
        }
    }

    return 0;
}

PUBLIC phys_bytes pgd_va2pa(pgdir_t* pgd, vir_bytes vir_addr)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];

    phys_bytes phys = pt[pt_index] & ARCH_VM_ADDR_MASK;

    return phys;
}

PUBLIC int unmap_memory(pgdir_t * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)vir_addr % ARCH_PG_SIZE != 0) printl("MM: map_memory: vir_addr is not page-aligned!\n");
    if (length % ARCH_PG_SIZE != 0) printl("MM: map_memory: length is not page-aligned!\n");

    while (1) {
        pt_mappage(pgd, NULL, vir_addr, 0);

        length -= ARCH_PG_SIZE;
        vir_addr += ARCH_PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}
