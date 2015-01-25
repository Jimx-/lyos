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
#include "apic.h"
#include <lyos/smp.h>

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
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
}

/* Temporarily map la in p's address space in kernel address space */
PRIVATE phys_bytes create_temp_map(struct proc * p, phys_bytes la, phys_bytes * len, int index, int * changed)
{
    /* the process is already in current page table */
    if (p && (p == get_cpulocal_var(pt_proc) || is_kerntaske(p->endpoint))) return la;

    pde_t pdeval;
    u32 pde = temppdes[index];
    if (p) {
        if (!p->seg.cr3_vir) panic("create_temp_map: proc cr3_vir not set");
        pdeval = p->seg.cr3_vir[ARCH_PDE(la)];
    } else {    /* physical address */
        pdeval = (la & ARCH_VM_ADDR_MASK_BIG) | 
            ARCH_PG_BIGPAGE | ARCH_PG_PRESENT | ARCH_PG_USER | ARCH_PG_RW;
    }

    if (!get_cpulocal_var(pt_proc)->seg.cr3_vir) panic("create_temp_map: pt_proc cr3_vir not set");
    if (get_cpulocal_var(pt_proc)->seg.cr3_vir[pde] != pdeval) {
        get_cpulocal_var(pt_proc)->seg.cr3_vir[pde] = pdeval;
        *changed = 1;
    }

    u32 offset = la & ARCH_VM_OFFSET_MASK_BIG;
    *len = min(*len, ARCH_BIG_PAGE_SIZE - offset);

    return pde * ARCH_BIG_PAGE_SIZE + offset; 
}

PRIVATE int la_la_copy(struct proc * p_dest, phys_bytes dest_la,
        struct proc * p_src, phys_bytes src_la, vir_bytes len)
{
    if (!get_cpulocal_var(pt_proc)) panic("pt_proc not present");
    if (read_cr3() != get_cpulocal_var(pt_proc)->seg.cr3_phys) panic("bad pt_proc cr3 value");

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

PRIVATE u32 get_phys32(phys_bytes phys_addr)
{
    u32 old_cr3 = read_cr3();
    u32 v;

    if (la_la_copy(proc_addr(KERNEL), &v, NULL, phys_addr, sizeof(v))) {
        panic("get_phys32: la_la_copy failed");
    }

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
    if (is_kerntaske(ep)) return (void *)((int)la - KERNEL_VMA);

    int slot;
    if (!verify_endpt(ep, &slot)) panic("la2pa: invalid endpoint");
    struct proc* p = proc_addr(slot);

    phys_bytes phys_addr = 0;
    unsigned long pgd_index = ARCH_PDE(la);
    unsigned long pt_index = ARCH_PTE(la);

    pde_t * pgd_phys = (pde_t *)(p->seg.cr3_phys);

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

#define KM_USERMAPPED   0
#define KM_LAPIC        1
#define KM_IOAPIC_FIRST 2

extern char _usermapped[], _eusermapped[];
PUBLIC vir_bytes usermapped_offset;

PRIVATE int KM_LAST = -1;
PRIVATE int KM_IOAPIC_END = -1;

extern u8 ioapic_enabled;
extern int nr_ioapics;
extern struct io_apic io_apics[MAX_IOAPICS];

/**
 * <Ring 0> Get kernel mapping information.
 */
PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    static int first = 1;

    if (first) {
        if (ioapic_enabled) {
            KM_IOAPIC_END = KM_IOAPIC_FIRST + nr_ioapics - 1;
            KM_LAST = KM_IOAPIC_END;
        }
        first = 0;
    }

    if (index > KM_LAST) return 1;
    
    if (index == KM_USERMAPPED) {
        *addr = (caddr_t)((char *)*(&_usermapped) - KERNEL_VMA);
        *len = (char *)*(&_eusermapped) - (char *)*(&_usermapped);
        *flags = KMF_USER;
        return 0;
    } 
#if CONFIG_X86_LOCAL_APIC
    if (index == KM_LAPIC) {
        if (!lapic_addr) return EINVAL;
        *addr = lapic_addr;
        *len = 4 << 10;
        *flags = KMF_WRITE;
        return 0;
    }
#endif
#if CONFIG_X86_IO_APIC
    if (ioapic_enabled && index >= KM_IOAPIC_FIRST && index <= KM_IOAPIC_END) {
        *addr = io_apics[index - KM_IOAPIC_FIRST].phys_addr;
        *len = 4 << 10;
        *flags = KMF_WRITE;
        return 0;
    }
#endif

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
        usermapped_offset = (char *)vir_addr - usermapped_start;
        sysinfo.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo *)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t *)USER_PTR(&kinfo);
        sysinfo.kern_log = (struct kern_log *)USER_PTR(&kern_log);

        if (syscall_style & SST_INTEL_SYSENTER) {
            printk("kernel: selecting intel SYSENTER syscall style\n");
            sysinfo.syscall_gate = (syscall_gate_t)USER_PTR(syscall_int);
        } else {
            sysinfo.syscall_gate = (syscall_gate_t)USER_PTR(syscall_int);
        }
        return 0;   
    }
#if CONFIG_X86_LOCAL_APIC
    if (index == KM_LAPIC) {
        lapic_vaddr = (vir_bytes)vir_addr;
        return 0;
    }
#endif
#if CONFIG_X86_IO_APIC
    if (index >= KM_IOAPIC_FIRST && index <= KM_IOAPIC_END) {
        io_apics[index - KM_IOAPIC_FIRST].vir_addr = (vir_bytes)vir_addr;
        return 0;
    }
#endif


    return 0;
}

PRIVATE void setcr3(struct proc * p, void * cr3, void * cr3_v)
{
    p->seg.cr3_phys = (u32)cr3;
    p->seg.cr3_vir = (u32 *)cr3_v;

    if (p->endpoint == TASK_MM) {
        wait_for_aps_to_finish_booting();

        write_cr3((u32)cr3);
        reload_cr3();
        get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
        /* using virtual address now */
        lapic_addr = lapic_vaddr;
        lapic_eoi_addr = LAPIC_EOI;
        int i;
        for (i = 0; i < nr_ioapics; i++) {
            io_apics[i].addr = io_apics[i].vir_addr;
        }
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
    struct proc * p_src = src_ep == NO_TASK ? NULL : endpt_proc(src_ep);
    struct proc * p_dest = dest_ep == NO_TASK ? NULL : endpt_proc(dest_ep);
    if ((src_ep != NO_TASK && p_src == NULL)
         || (dest_ep != NO_TASK && p_dest == NULL)) return EINVAL;

    return la_la_copy(p_dest, (phys_bytes)dest_addr, p_src, (phys_bytes)src_addr, len);
}
