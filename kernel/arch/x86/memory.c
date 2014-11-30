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
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>

extern int syscall_style;

/* temporary mappings */
#define MAX_TEMPPDES    2
#define TEMPPDE_SRC     0
#define TEMPPDE_DST     1
PRIVATE u32 temppdes[MAX_TEMPPDES];

PUBLIC void init_memory()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        temppdes[i] = kinfo.kernel_end_pde++;
    }
}

/* Temporarily map la in p's address space in kernel address space */
PRIVATE phys_bytes create_temp_map(struct proc * p, phys_bytes la, phys_bytes * len, int index, int * changed)
{
    /* the process is already in current page table */
    if (p == get_cpulocal_var(proc_ptr)) return la;

    pde_t pdeval;
    u32 pde = temppdes[index];
    if (p) {
        pdeval = p->seg.cr3_vir[ARCH_PDE(la)];
    } else {    /* physical address */
        pdeval = (la & ARCH_VM_ADDR_MASK_BIG) | 
            ARCH_PG_BIGPAGE | ARCH_PG_PRESENT | ARCH_PG_USER | ARCH_PG_RW;
    }

    if (get_cpulocal_var(proc_ptr)->seg.cr3_vir[pde] != pdeval) {
        get_cpulocal_var(proc_ptr)->seg.cr3_vir[pde] = pdeval;
        *changed = 1;
    }

    u32 offset = la & ARCH_VM_OFFSET_MASK_BIG;
    *len = min(*len, ARCH_BIG_PAGE_SIZE - offset);

    return pde * ARCH_BIG_PAGE_SIZE + offset; 
}

PRIVATE int la_la_copy(struct proc * p_dest, phys_bytes dest_la,
        struct proc * p_src, phys_bytes src_la, vir_bytes len)
{
    while (len > 0) {
        vir_bytes chunk = len;
        phys_bytes src_mapped, dest_mapped;
        int changed = 0;

        src_mapped = create_temp_map(p_src, src_la, &chunk, TEMPPDE_SRC, &changed);
        dest_mapped = create_temp_map(p_dest, dest_la, &chunk, TEMPPDE_DST, &changed);

        if (changed) reload_cr3();

        phys_copy((void *)dest_mapped, (void *)src_mapped, chunk);

        len -= chunk;
        src_la += chunk;
        dest_la += chunk;
    }

    return 0;
}

PRIVATE void * find_free_pages(pde_t * pgd_phys, int nr_pages, void * start, void * end)
{
    /* default value */
    if (start == NULL) start = (void *)FIXMAP_START;
    if (end == NULL) end = (void *)FIXMAP_END;

    unsigned int start_pde = ARCH_PDE((unsigned int)start);
    unsigned int end_pde = ARCH_PDE((unsigned int)end);

    int i, j;
    pde_t * vir_pts = (pde_t *)((u32)pgd_phys + KERNEL_VMA);
    int allocated_pages = 0;
    void * retaddr = NULL;
    for (i = start_pde; i < end_pde; i++) {
        pte_t * pt_entries = (pte_t *)(((pde_t)vir_pts[i] + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
        /* the pde is empty, we have I386_VM_DIR_ENTRIES free pages */
        if (pt_entries == NULL) {
            nr_pages -= I386_VM_DIR_ENTRIES;
            allocated_pages += I386_VM_DIR_ENTRIES;
            if (retaddr == NULL) retaddr = (void *)ARCH_VM_ADDRESS(i, 0, 0);
            if (nr_pages <= 0) {
                return retaddr;
            }
            continue;
        }

        for (j = 0; j < I386_VM_DIR_ENTRIES; j++) {
            if (pt_entries[j] != 0) {
                nr_pages += allocated_pages;
                retaddr = NULL;
                allocated_pages = 0;
            } else {
                nr_pages--;
                allocated_pages++;
                if (!retaddr) retaddr = (void *)ARCH_VM_ADDRESS(i, j, 0);
            }

            if (nr_pages <= 0) return retaddr;
        }
    }

    return NULL;
}

PRIVATE u32 get_phys32(phys_bytes phys_addr)
{
    u32 v;

    //if (la_la_copy(get_cpulocal_var(proc_ptr), &v, NULL, phys_addr, sizeof(v))) {
    //    panic("get_phys32: la_la_copy failed");
    //}

    //return v;
    u32 old_cr3 = read_cr3();
    write_cr3((u32)initial_pgd);
    reload_cr3();

    u32 temp_addr = (u32)find_free_pages(initial_pgd, 1, (void*)FIXMAP_START, (void*)FIXMAP_END);
    pde_t * initial_pgd_vir = (pde_t *)((u32)initial_pgd + KERNEL_VMA);

    unsigned long pgd_index = ARCH_PDE(temp_addr);
    unsigned long pt_index = ARCH_PTE(temp_addr);

    pte_t * pt = (pte_t *)(((pde_t)initial_pgd_vir[pgd_index] + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
    pt[pt_index] = ((u32)phys_addr & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;

    temp_addr += phys_addr & ARCH_VM_OFFSET_MASK;
    memcpy(&v, (void *)temp_addr, sizeof(v));

    pt[pt_index] = 0;

    write_cr3(old_cr3);
    reload_cr3();
    
    return v;
}

/*****************************************************************************
 *                va2la
 *****************************************************************************/
/**
 * <Ring 0~1> Virtual addr --> Linear addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param va   Virtual address.
 * 
 * @return The linear address for the given virtual address.
 *****************************************************************************/
PUBLIC void* va2la(endpoint_t ep, void* va)
{
    return va;
}

/*****************************************************************************
 *                la2pa
 *****************************************************************************/
/**
 * <Ring 0~1> Linear addr --> physical addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param la   Linear address.
 * 
 * @return The physical address for the given linear address.
 *****************************************************************************/
PUBLIC void * la2pa(endpoint_t ep, void * la)
{
    int slot;
    if (!verify_endpt(ep, &slot)) panic("la2pa: invalid endpoint");
    struct proc* p = proc_addr(slot);

    phys_bytes phys_addr = 0;
    unsigned long pgd_index = ARCH_PDE(la);
    unsigned long pt_index = ARCH_PTE(la);

    pde_t * pgd_phys = (pde_t *)(p->seg.cr3_phys);

    //pte_t * pt = p->pgd.vir_pts[pgd_index];
    //return (void*)((pt[pt_index] & ARCH_VM_ADDR_MASK) + ((int)la & ARCH_VM_OFFSET_MASK));
    /*if (pgd_phys == NULL) {
        pte_t * pt = p->pgd.vir_pts[pgd_index];
        return (void*)((pt[pt_index] & ARCH_VM_ADDR_MASK) + ((int)la & ARCH_VM_OFFSET_MASK));
    } else*/ {
        pde_t pde_v = (pde_t)get_phys32((phys_bytes)(pgd_phys + pgd_index));

        if (pde_v & ARCH_PG_BIGPAGE) {
            phys_addr = pde_v & ARCH_VM_ADDR_MASK_BIG;
            phys_addr += (phys_bytes)la & ARCH_VM_OFFSET_MASK_BIG;
            return (void *)phys_addr;
        }

        pte_t * pt_entries = (pte_t *)(pde_v & ARCH_VM_ADDR_MASK);
        pte_t pte_v = (pte_t)get_phys32((phys_bytes)(pt_entries + pt_index));
        return (void*)((pte_v & ARCH_VM_ADDR_MASK) + ((int)la & ARCH_VM_OFFSET_MASK));
    }
}

/*****************************************************************************
 *                va2pa
 *****************************************************************************/
/**
 * <Ring 0~1> Virtual addr --> physical addr.
 * 
 * @param pid  PID of the proc whose address is to be calculated.
 * @param va   Virtual address.
 * 
 * @return The physical address for the given virtual address.
 *****************************************************************************/
PUBLIC void * va2pa(endpoint_t ep, void * va)
{
    return la2pa(ep, va2la(ep, va));
}

#define KM_USERMAPPED  0
#define KM_LAST     KM_USERMAPPED

extern char _usermapped[], _eusermapped[];

/**
 * <Ring 0> Get kernel mapping information.
 */
PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    if (index > KM_LAST) return 1;
    
    if (index == KM_USERMAPPED) {
        *addr = (caddr_t)((char *)*(&_usermapped) - KERNEL_VMA);
        *len = (char *)*(&_eusermapped) - (char *)*(&_usermapped);
        *flags = KMF_USER;
    }

    return 0;
}

/**
 * <Ring 0> Set kernel mapping information according to MM's parameter.
 */
PUBLIC int arch_reply_kern_mapping(int index, void * vir_addr)
{
    char * usermapped_start = (char *)*(&_usermapped);

#define USER_PTR(x) (((char *)(x) - usermapped_start) + (char *)vir_addr)

    if (index == KM_USERMAPPED) {
        sysinfo.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo *)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t *)USER_PTR(&kinfo);
        sysinfo.kern_log = (struct kern_log *)USER_PTR(&kern_log);

        if (syscall_style & SST_INTEL_SYSENTER) {
            printk("kernel: selecting intel SYSENTER syscall style\n");
        }
        sysinfo.syscall_gate = (syscall_gate_t)USER_PTR(syscall_int);
    }

    return 0;
}

PRIVATE void setcr3(struct proc * p, void * cr3, void * cr3_v)
{
    p->seg.cr3_phys = (u32)cr3;
    p->seg.cr3_vir = (u32 *)cr3_v;

    if (p->endpoint == TASK_MM) {
        write_cr3((u32)cr3);
        reload_cr3();
    }

    PST_UNSET(p, PST_MMINHIBIT);
}

PUBLIC int arch_vmctl(MESSAGE * m, struct proc * p)
{
    int request = m->VMCTL_REQUEST;

    switch (request) {
    case VMCTL_GETPDBR:
        m->VMCTL_VALUE = p->seg.cr3_phys;
        return 0;
    case VMCTL_SET_ADDRESS_SPACE:
        setcr3(p, m->VMCTL_PHYS_ADDR, m->VMCTL_VIR_ADDR);
        return 0;
    }

    return EINVAL;
}

PUBLIC int vir_copy(endpoint_t dest_ep, void * dest_addr,
                        endpoint_t src_ep, void * src_addr, int len)
{
    //struct proc * p_src = proc_addr(src_pid);
    //struct proc * p_dest = proc_addr(dest_pid);

    //return la_la_copy(p_dest, (phys_bytes)dest_addr, p_src, (phys_bytes)src_addr, len);

    u32 old_cr3 = read_cr3();
    pde_t * initial_pgd_vir = (pde_t *)((u32)initial_pgd + KERNEL_VMA);

#define PGD_ENTRY(i) (pte_t *)(((pde_t)initial_pgd_vir[i] + KERNEL_VMA) & ARCH_VM_ADDR_MASK)

    write_cr3((u32)initial_pgd);
    reload_cr3();

    /* map in dest address */
    u32 dest_la = (u32)va2la(dest_ep, dest_addr);
    u32 dest_offset = dest_la % PG_SIZE, dest_len = len;
    if (dest_offset != 0) {
        dest_la -= dest_offset;
        dest_len += dest_offset;
    }

    if (dest_len % PG_SIZE != 0) {
        dest_len += PG_SIZE - (dest_len % PG_SIZE);
    }

    int dest_pages = dest_len / PG_SIZE;
    u32 dest_vaddr = (u32)find_free_pages(initial_pgd, dest_pages, (void*)FIXMAP_START, (void*)FIXMAP_END);
    if (dest_addr == NULL) return ENOMEM;

    int i;
    u32 _dest_vaddr = dest_vaddr;
    u32 _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = ((u32)la2pa(dest_ep, (void *)_dest_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
    }

    /* map in source address */
    u32 src_la = (u32)va2la(src_ep, src_addr);
    u32 src_offset = src_la % PG_SIZE, src_len = len;
    if (src_offset != 0) {
        src_la -= src_offset;
        src_len += src_offset;
    }

    if (src_len % PG_SIZE != 0) {
        src_len += PG_SIZE - (src_len % PG_SIZE);
    }

    u32 src_pages = src_len / PG_SIZE;
    u32 src_vaddr = (u32)find_free_pages(initial_pgd, src_pages, (void*)FIXMAP_START, (void*)FIXMAP_END);
    if (src_addr == NULL) return ENOMEM;

    u32 _src_vaddr = src_vaddr;
    u32 _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = ((u32)la2pa(src_ep, (void *)_src_la) & ARCH_VM_ADDR_MASK) | PG_PRESENT | PG_RW | PG_USER;
    }

    //void * src_pa = (void *)va2pa(src_pid, src_addr);
    //void * dest_pa = (void *)va2pa(dest_pid, dest_addr);

    //phys_copy(dest_pa, src_pa, len);
    phys_copy((void *)(dest_vaddr + dest_offset), (void *)(src_vaddr + src_offset), len);

    /* unmap */
    _dest_vaddr = dest_vaddr;
    _dest_la = dest_la;
    for (i = 0; i < dest_pages; i++, _dest_la += PG_SIZE, _dest_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_dest_vaddr);
        unsigned long pt_index = ARCH_PTE(_dest_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = 0;
    }

    _src_vaddr = src_vaddr;
    _src_la = src_la;
    for (i = 0; i < src_pages; i++, _src_la += PG_SIZE, _src_vaddr += PG_SIZE)
    {
        unsigned long pgd_index = ARCH_PDE(_src_vaddr);
        unsigned long pt_index = ARCH_PTE(_src_vaddr);

        pte_t * pt = PGD_ENTRY(pgd_index);
        pt[pt_index] = 0;
    }

    write_cr3(old_cr3);
    reload_cr3();

    return 0;
}
