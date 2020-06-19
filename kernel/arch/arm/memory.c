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

#include <lyos/type.h>
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
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>
#include <lyos/smp.h>
#include "arch.h"

extern int _KERN_OFFSET;

/* temporary mappings */
#define MAX_TEMPPDES 2
#define TEMPPDE_SRC 0
#define TEMPPDE_DST 1
static u32 temppdes[MAX_TEMPPDES];

#define _SRC_ 0
#define _DEST_ 1
#define EFAULT_SRC 1
#define EFAULT_DEST 2

void init_memory()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        temppdes[i] = kinfo.kernel_end_pde++;
    }
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
}

void clear_memcache()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        struct proc* p = get_cpulocal_var(pt_proc);
        int pde = temppdes[i];
        p->seg.ttbr_vir[pde] = 0;
    }
}

/* Temporarily map la in p's address space in kernel address space */
static phys_bytes create_temp_map(struct proc* p, phys_bytes la,
                                  phys_bytes* len, int index, int* changed)
{
    /* the process is already in current page table */
    if (p && (p == get_cpulocal_var(pt_proc) || is_kerntaske(p->endpoint)))
        return la;

    pde_t pdeval;
    u32 pde = temppdes[index];
    if (p) {
        if (!p->seg.ttbr_vir) panic("create_temp_map: proc ttbr_vir not set");
        pdeval = p->seg.ttbr_vir[ARCH_PDE(la)];
    } else { /* physical address */
        pdeval = (la & ARM_VM_SECTION_MASK) | ARM_VM_SECTION |
                 ARM_VM_SECTION_DOMAIN | ARM_VM_SECTION_CACHED |
                 ARM_VM_SECTION_USER;
    }

    if (!get_cpulocal_var(pt_proc)->seg.ttbr_vir)
        panic("create_temp_map: pt_proc ttbr_vir not set");
    if (get_cpulocal_var(pt_proc)->seg.ttbr_vir[pde] != pdeval) {
        get_cpulocal_var(pt_proc)->seg.ttbr_vir[pde] = pdeval;
        *changed = 1;
    }

    u32 offset = la & ARM_VM_OFFSET_MASK_1MB;
    *len = min(*len, ARM_SECTION_SIZE - offset);

    return pde * ARM_SECTION_SIZE + offset;
}

static int la_la_copy(struct proc* p_dest, phys_bytes dest_la,
                      struct proc* p_src, phys_bytes src_la, size_t len)
{
    if (!get_cpulocal_var(pt_proc)) panic("pt_proc not present");
    if (read_ttbr0() != get_cpulocal_var(pt_proc)->seg.ttbr_phys)
        panic("bad pt_proc ttbr0 value");

    while (len > 0) {
        size_t chunk = len;
        phys_bytes src_mapped, dest_mapped;
        int changed = 0;

        src_mapped =
            create_temp_map(p_src, src_la, &chunk, TEMPPDE_SRC, &changed);
        dest_mapped =
            create_temp_map(p_dest, dest_la, &chunk, TEMPPDE_DST, &changed);

        if (changed) reload_ttbr0();

        phys_bytes fault_addr =
            memcpy((void*)dest_mapped, (void*)src_mapped, chunk);

        /*if (fault_addr) {
            if (fault_addr >= src_mapped && fault_addr < src_mapped + chunk)
                return EFAULT_SRC;
            if (fault_addr >= dest_mapped && fault_addr < dest_mapped + chunk)
                return EFAULT_DEST;
            return EFAULT;
        }*/

        len -= chunk;
        src_la += chunk;
        dest_la += chunk;
    }

    return 0;
}

static u32 get_phys32(phys_bytes phys_addr)
{
    u32 v;

    if (la_la_copy(proc_addr(KERNEL), &v, NULL, phys_addr, sizeof(v))) {
        panic("get_phys32: la_la_copy failed");
    }

    return v;
}

void* va2pa(endpoint_t ep, void* va)
{
    phys_bytes la = (phys_bytes)va;
    if (is_kerntaske(ep)) return (void*)(la - (phys_bytes)&_KERN_OFFSET);

    int slot;
    if (!verify_endpt(ep, &slot)) panic("va2pa: invalid endpoint");
    struct proc* p = proc_addr(slot);

    phys_bytes phys_addr = 0;
    unsigned long pgd_index = ARCH_PDE(la);
    unsigned long pt_index = ARCH_PTE(la);

    pde_t* pgd_phys = (pde_t*)(p->seg.ttbr_phys);

    pde_t pde_v = (pde_t)get_phys32((phys_bytes)(pgd_phys + pgd_index));

    if (pde_v & ARM_VM_SECTION) {
        phys_addr = pde_v & ARM_VM_SECTION_MASK;
        phys_addr += la & ARM_VM_OFFSET_MASK_1MB;
        return (void*)phys_addr;
    }

    pte_t* pt_entries = (pte_t*)(pde_v & ARCH_PG_MASK);
    pte_t pte_v = (pte_t)get_phys32((phys_bytes)(pt_entries + pt_index));
    return (void*)((pte_v & ARCH_PG_MASK) + ((uintptr_t)la % ARCH_PG_SIZE));
}

#define MAX_KERN_MAPPINGS 8

struct kern_map {
    phys_bytes phys_addr;
    phys_bytes len;
    int flags;
    void* vir_addr;
    void** mapped_addr;
};

static struct kern_map kern_mappings[MAX_KERN_MAPPINGS];
static int kern_mapping_count = 0;

int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags,
                  void** mapped_addr)
{
    if (kern_mapping_count >= MAX_KERN_MAPPINGS) return ENOMEM;

    struct kern_map* pkm = &kern_mappings[kern_mapping_count++];
    pkm->phys_addr = phys_addr;
    pkm->len = len;
    pkm->flags = flags;
    pkm->mapped_addr = mapped_addr;

    return 0;
}

#define KM_USERMAPPED 0
#define KM_KERN_MAPPING 1

extern char _usermapped[], _eusermapped[];
off_t usermapped_offset;

int arch_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags)
{
    if (index >= KM_KERN_MAPPING + kern_mapping_count) return 1;

    if (index == KM_USERMAPPED) {
        *addr = (caddr_t)((char*)*(&_usermapped) - (phys_bytes)&_KERN_OFFSET);
        *len = (char*)*(&_eusermapped) - (char*)*(&_usermapped);
        *flags = KMF_USER;
        return 0;
    }

    if (index >= KM_KERN_MAPPING &&
        index < KM_KERN_MAPPING + kern_mapping_count) {
        struct kern_map* pkm = &kern_mappings[index - KM_KERN_MAPPING];
        *addr = (caddr_t)pkm->phys_addr;
        *len = pkm->len;
        *flags = pkm->flags;
        return 0;
    }

    return 0;
}

int arch_reply_kern_mapping(int index, void* vir_addr)
{
    char* usermapped_start = (char*)*(&_usermapped);

#define USER_PTR(x) (((char*)(x)-usermapped_start) + (char*)vir_addr)

    if (index == KM_USERMAPPED) {
        usermapped_offset = (char*)vir_addr - usermapped_start;
        sysinfo.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo*)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t*)USER_PTR(&kinfo);
        sysinfo.kern_log = (struct kern_log*)USER_PTR(&kern_log);
        sysinfo.machine = (struct machine*)USER_PTR(&machine);
        return 0;
    }

    if (index >= KM_KERN_MAPPING &&
        index < KM_KERN_MAPPING + kern_mapping_count) {
        kern_mappings[index - KM_KERN_MAPPING].vir_addr = vir_addr;
        return 0;
    }

    return 0;
}

static void setttbr0(struct proc* p, void* ttbr, void* ttbr_v)
{
    p->seg.ttbr_phys = (u32)ttbr;
    p->seg.ttbr_vir = (u32*)ttbr_v;

    if (p->endpoint == TASK_MM) {
        int i;
        /* update mapped address of kernel mappings */
        for (i = 0; i < kern_mapping_count; i++) {
            struct kern_map* pkm = &kern_mappings[i];
            *(pkm->mapped_addr) = pkm->vir_addr;
        }

        write_ttbr0((u32)ttbr);
        get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
    }

    PST_UNSET(p, PST_MMINHIBIT);
}

int arch_vmctl(MESSAGE* m, struct proc* p)
{
    int request = m->VMCTL_REQUEST;

    switch (request) {
    case VMCTL_GETPDBR:
        m->VMCTL_VALUE = p->seg.ttbr_phys;
        return 0;
    case VMCTL_SET_ADDRESS_SPACE:
        setttbr0(p, m->VMCTL_PHYS_ADDR, m->VMCTL_VIR_ADDR);
        return 0;
    }

    return EINVAL;
}

void mm_suspend(struct proc* caller, endpoint_t target, void* laddr,
                size_t bytes, int write, int type)
{
    PST_SET_LOCKED(caller, PST_MMREQUEST);

    caller->mm_request.req_type = MMREQ_CHECK;
    caller->mm_request.type = type;
    caller->mm_request.target = target;
    caller->mm_request.params.check.start = laddr;
    caller->mm_request.params.check.len = bytes;
    caller->mm_request.params.check.write = write;

    caller->mm_request.next_request = mmrequest;
    mmrequest = caller;
    if (!caller->mm_request.next_request) {
        if (send_sig(TASK_MM, SIGKMEM) != 0)
            panic("mm_suspend: send_sig failed");
    }
}

int _vir_copy(struct proc* caller, struct vir_addr* dest_addr,
              struct vir_addr* src_addr, void* bytes, int check)
{
    struct vir_addr* vir_addrs[2];
    struct proc* procs[2];

    if (bytes < 0) return EINVAL;

    vir_addrs[_SRC_] = src_addr;
    vir_addrs[_DEST_] = dest_addr;

    int i;
    for (i = _SRC_; i <= _DEST_; i++) {
        endpoint_t proc_ep = vir_addrs[i]->proc_ep;

        procs[i] = proc_ep == NO_TASK ? NULL : endpt_proc(proc_ep);

        if (proc_ep != NO_TASK && procs[i] == NULL) return ESRCH;
    }

    int retval = la_la_copy(procs[_DEST_], vir_addrs[_DEST_]->addr,
                            procs[_SRC_], vir_addrs[_SRC_]->addr, bytes);

    if (retval) {
        if (retval == EFAULT) return EFAULT;

        if (retval != EFAULT_SRC && retval != EFAULT_DEST)
            panic("vir_copy: la_la_copy failed");

        if (!check || !caller) return EFAULT;

        int write;
        void* fault_la;
        endpoint_t target;
        if (retval == EFAULT_SRC) {
            target = vir_addrs[_SRC_]->proc_ep;
            fault_la = vir_addrs[_SRC_]->addr;
            write = 0;
        } else if (retval == EFAULT_DEST) {
            target = vir_addrs[_DEST_]->proc_ep;
            fault_la = vir_addrs[_DEST_]->addr;
            write = 1;
        }

        mm_suspend(caller, target, fault_la, bytes, write, MMREQ_TYPE_SYSCALL);
        return MMSUSPEND;
    }

    return 0;
}

int _data_vir_copy(struct proc* caller, endpoint_t dest_ep, void* dest_addr,
                   endpoint_t src_ep, void* src_addr, int len, int check)
{
    struct vir_addr src, dest;

    src.addr = src_addr;
    src.proc_ep = src_ep;

    dest.addr = dest_addr;
    dest.proc_ep = dest_ep;

    if (check)
        return vir_copy_check(caller, &dest, &src, len);
    else
        return vir_copy(&dest, &src, len);
}
