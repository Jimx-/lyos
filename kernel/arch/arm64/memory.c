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
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "asm/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>
#include <lyos/smp.h>
#include <asm/pagetable.h>

/* temporary mappings */
#define MAX_TEMPPDES 2
#define TEMPPDE_SRC  0
#define TEMPPDE_DST  1
static unsigned long temppdes[MAX_TEMPPDES];

#define _SRC_       0
#define _DEST_      1
#define EFAULT_SRC  1
#define EFAULT_DEST 2

static inline void set_pde(pde_t* pdep, pde_t pde)
{
    *(volatile pde_t*)pdep = pde;
}

void init_memory()
{
    int i;
    int temppde_start;

    temppde_start = ARCH_VM_DIR_ENTRIES - MAX_TEMPPDES;

    for (i = 0; i < MAX_TEMPPDES; i++) {
        temppdes[i] = temppde_start++;
    }
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
}

void clear_memcache()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        set_pde(&init_pg_dir[temppdes[i]], __pde(0));
    }
    flush_tlb();
}

/* Temporarily map la in p's address space in kernel address space */
static void* create_temp_map(struct proc* p, void* la, size_t* len, int index,
                             int* changed)
{
    phys_bytes pa;
    pde_t pdeval;
    unsigned long pde = temppdes[index];

    /* the process is already in current page table */
    if (p && (p == get_cpulocal_var(pt_proc) || is_kerntaske(p->endpoint)))
        return la;

    if (p) {
        if (!p->seg.ttbr_vir) panic("create_temp_map: proc ttbr_vir not set");
        pdeval = __pde(p->seg.ttbr_vir[ARCH_PDE(la)]);
    } else { /* physical address */
        pa = (phys_bytes)la;
        return __va(pa);
    }

    if (pde_val(init_pg_dir[pde]) != pde_val(pdeval)) {
        set_pde(&init_pg_dir[pde], pdeval);
        *changed = 1;
    }

    unsigned long offset = ((uintptr_t)la) % ARCH_PGD_SIZE;
    *len = min(*len, ARCH_PGD_SIZE - offset);

    return (void*)(uintptr_t)(-((ARCH_VM_DIR_ENTRIES - pde) << ARCH_PGD_SHIFT) +
                              offset);
}

static int la_la_copy(struct proc* p_dest, void* dest_la, struct proc* p_src,
                      void* src_la, size_t len)
{
    if (!get_cpulocal_var(pt_proc)) panic("pt_proc not present");

    while (len > 0) {
        size_t chunk = len;
        void *src_mapped, *dest_mapped;
        int changed = 0;

        src_mapped =
            create_temp_map(p_src, src_la, &chunk, TEMPPDE_SRC, &changed);
        dest_mapped =
            create_temp_map(p_dest, dest_la, &chunk, TEMPPDE_DST, &changed);

        if (changed) flush_tlb();

        void* fault_addr = phys_copy(dest_mapped, src_mapped, chunk);

        if (fault_addr) {
            if (fault_addr >= src_mapped && fault_addr < src_mapped + chunk)
                return EFAULT_SRC;
            if (fault_addr >= dest_mapped && fault_addr < dest_mapped + chunk)
                return EFAULT_DEST;
            return EFAULT;
        }

        len -= chunk;
        src_la += chunk;
        dest_la += chunk;
    }

    return 0;
}

phys_bytes va2pa(endpoint_t ep, void* va) {}

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

int arch_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags) {}

int arch_reply_kern_mapping(int index, void* vir_addr) {}

int arch_vmctl(MESSAGE* m, struct proc* p) {}

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
              struct vir_addr* src_addr, size_t bytes, int check)
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
