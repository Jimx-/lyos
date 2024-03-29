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
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/param.h>
#include <lyos/vm.h>
#include <asm/page.h>
#include <asm/pagetable.h>
#include <kernel/watchdog.h>
#include <errno.h>
#include "apic.h"
#include <lyos/smp.h>

extern int syscall_style;

/* temporary mappings */
#define MAX_TEMPPDES 2
#define TEMPPDE_SRC  0
#define TEMPPDE_DST  1
static unsigned long temppdes[MAX_TEMPPDES];

#define _SRC_       0
#define _DEST_      1
#define EFAULT_SRC  1
#define EFAULT_DEST 2

int send_sig(endpoint_t ep, int signo);

void init_memory()
{
    int i;
    int temppde_start;

#ifdef CONFIG_X86_32
    temppde_start = ARCH_PDE(PKMAP_END);
#else
    temppde_start = ARCH_PDE(PKMAP_START) - MAX_TEMPPDES;
#endif

    for (i = 0; i < MAX_TEMPPDES; i++) {
        temppdes[i] = temppde_start++;
    }
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
}

void clear_memcache()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        struct proc* p = get_cpulocal_var(pt_proc);
        int pde = temppdes[i];
        p->seg.cr3_vir[pde] = 0;
    }
}

/* Temporarily map la in p's address space in kernel address space */
static void* create_temp_map(struct proc* p, void* la, size_t* len, int index,
                             int* changed)
{
    /* the process is already in current page table */
    if (p && (p == get_cpulocal_var(pt_proc) || is_kerntaske(p->endpoint)))
        return la;

    phys_bytes pa;
    pde_t pdeval;
    unsigned long pde = temppdes[index];

    if (p) {
        if (!p->seg.cr3_vir) panic("create_temp_map: proc cr3_vir not set");
        pdeval = __pde(p->seg.cr3_vir[ARCH_PDE(la)]);
    } else { /* physical address */
        pa = (phys_bytes)la;

#ifdef CONFIG_X86_32
        if (pa < LOWMEM_END) { /* low memory */
            *len = min(*len, LOWMEM_END - pa);
            return __va(pa);
        }

        pdeval =
            __pde((((uintptr_t)la) & I386_VM_ADDR_MASK_4MB) | ARCH_PG_BIGPAGE |
                  ARCH_PG_PRESENT | ARCH_PG_USER | ARCH_PG_RW);
#else
        return __va(pa);
#endif
    }

    if (!get_cpulocal_var(pt_proc)->seg.cr3_vir)
        panic("create_temp_map: pt_proc cr3_vir not set");
    if (get_cpulocal_var(pt_proc)->seg.cr3_vir[pde] != pde_val(pdeval)) {
        get_cpulocal_var(pt_proc)->seg.cr3_vir[pde] = pde_val(pdeval);
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
    if (read_cr3() != get_cpulocal_var(pt_proc)->seg.cr3_phys)
        panic("bad pt_proc cr3 value");

    while (len > 0) {
        size_t chunk = len;
        void *src_mapped, *dest_mapped;
        int changed = 0;

        src_mapped =
            create_temp_map(p_src, src_la, &chunk, TEMPPDE_SRC, &changed);
        dest_mapped =
            create_temp_map(p_dest, dest_la, &chunk, TEMPPDE_DST, &changed);

        if (changed) reload_cr3();

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
phys_bytes va2pa(endpoint_t ep, void* va)
{
    pde_t* pde;
    pud_t* pude;
    pmd_t* pmde;
    pte_t* pte;
    int slot;
    struct proc* p;

    if (is_kerntaske(ep)) return __pa(va);

    if (!verify_endpt(ep, &slot)) panic("va2pa: invalid endpoint");
    p = proc_addr(slot);

    pde = pgd_offset((pde_t*)p->seg.cr3_vir, (unsigned long)va);
    if (pde_none(*pde)) {
        return -1;
    }

    pude = pud_offset(pde, (unsigned long)va);
    if (pude_none(*pude)) {
        return -1;
    }

    pmde = pmd_offset(pude, (unsigned long)va);
    if (pmde_none(*pmde)) {
        return -1;
    }

    pte = pte_offset(pmde, (unsigned long)va);
    if (!pte_present(*pte)) {
        return -1;
    }

    return (pte_pfn(*pte) << ARCH_PG_SHIFT) + ((uintptr_t)va % ARCH_PG_SIZE);
}

#define KM_USERMAPPED   0
#define KM_LAPIC        1
#define KM_IOAPIC_FIRST 2

extern char _usermapped[], _eusermapped[];
off_t usermapped_offset;

static int KM_LAST = -1;
static int KM_IOAPIC_END = -1;
static int KM_HPET = -1;

extern u8 ioapic_enabled;
extern int nr_ioapics;
extern struct io_apic io_apics[MAX_IOAPICS];
extern void *hpet_addr, *hpet_vaddr;
/**
 * <Ring 0> Get kernel mapping information.
 */
int arch_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags)
{
    static int first = 1;

    if (first) {
        if (ioapic_enabled) {
            KM_IOAPIC_END = KM_IOAPIC_FIRST + nr_ioapics - 1;
            if (hpet_addr)
                KM_HPET = KM_IOAPIC_END + 1;
            else
                KM_HPET = KM_IOAPIC_END;
            KM_LAST = KM_HPET;
        }
        first = 0;
    }

    if (index > KM_LAST) return 1;

    if (index == KM_USERMAPPED) {
        *addr = (caddr_t)((char*)*(&_usermapped) - KERNEL_VMA);
        *len = (char*)*(&_eusermapped) - (char*)*(&_usermapped);
        *flags = KMF_USER;
        return 0;
    }

#if CONFIG_X86_LOCAL_APIC
    if (index == KM_LAPIC) {
        if (!lapic_addr) return EINVAL;
        *addr = (caddr_t)lapic_addr;
        *len = 4 << 10;
        *flags = KMF_WRITE;
        return 0;
    }
#endif
#if CONFIG_X86_IO_APIC
    if (ioapic_enabled && index >= KM_IOAPIC_FIRST && index <= KM_IOAPIC_END) {
        *addr = (caddr_t)io_apics[index - KM_IOAPIC_FIRST].phys_addr;
        *len = 4 << 10;
        *flags = KMF_WRITE;
        return 0;
    }
#endif
    if (hpet_addr && index == KM_HPET) {
        *addr = (caddr_t)hpet_addr;
        *len = 4 << 10;
        *flags = KMF_WRITE;
        return 0;
    }

    return 0;
}

/**
 * <Ring 0> Set kernel mapping information according to MM's parameter.
 */
int arch_reply_kern_mapping(int index, void* vir_addr)
{
    char* usermapped_start = (char*)*(&_usermapped);

#define USER_PTR(x) (((char*)(x)-usermapped_start) + (char*)vir_addr)

    if (index == KM_USERMAPPED) {
        usermapped_offset = (char*)vir_addr - usermapped_start;
        sysinfo.user_info.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo*)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t*)USER_PTR(&kinfo);
        sysinfo.kern_log = (struct kern_log*)USER_PTR(&kern_log);
        sysinfo.machine = (struct machine*)USER_PTR(&machine);
        sysinfo.user_info.clock_info =
            (struct kclockinfo*)USER_PTR(&kclockinfo);

        if (syscall_style & SST_AMD_SYSCALL) {
            printk("kernel: selecting AMD SYSCALL syscall style\n");
            sysinfo.user_info.syscall_gate =
                (syscall_gate_t)USER_PTR(syscall_syscall);
        } else if (syscall_style & SST_INTEL_SYSENTER) {
            printk("kernel: selecting intel SYSENTER syscall style\n");
            sysinfo.user_info.syscall_gate =
                (syscall_gate_t)USER_PTR(syscall_sysenter);
        } else {
            sysinfo.user_info.syscall_gate =
                (syscall_gate_t)USER_PTR(syscall_int);
        }
        return 0;
    }
#if CONFIG_X86_LOCAL_APIC
    if (index == KM_LAPIC) {
        lapic_vaddr = vir_addr;
        return 0;
    }
#endif
#if CONFIG_X86_IO_APIC
    if (index >= KM_IOAPIC_FIRST && index <= KM_IOAPIC_END) {
        io_apics[index - KM_IOAPIC_FIRST].vir_addr = (void*)vir_addr;
        return 0;
    }
#endif
    if (index == KM_HPET) {
        hpet_vaddr = vir_addr;
    }

    if (index == KM_LAST) {
        /* At this point, all physical regions required by the kernel have been
         * mapped into the virtual address space by MM and the kernel no longer
         * need the identity mapping to access physical memory locations. */
        pg_unmap_identity(initial_pgd);
    }

    return 0;
}

static void setcr3(struct proc* p, unsigned long cr3, void* cr3_v)
{
    p->seg.cr3_phys = cr3;
    p->seg.cr3_vir = (unsigned long*)cr3_v;

    if (p->endpoint == TASK_MM) {
#if CONFIG_SMP
        wait_for_aps_to_finish_booting();
        cmb();
#endif

        write_cr3((u32)cr3);
        reload_cr3();
        get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
        /* using virtual address now */
        lapic_addr = lapic_vaddr;
        lapic_eoi_addr = lapic_addr + LAPIC_EOI;
        int i;
        for (i = 0; i < nr_ioapics; i++) {
            io_apics[i].addr = io_apics[i].vir_addr;
        }
        hpet_addr = hpet_vaddr;

        if (watchdog_enabled) {
            if (arch_watchdog_init() != 0) {
                watchdog_enabled = 0;
            }
        }

        smp_commence();
    }

    PST_UNSET(p, PST_MMINHIBIT);
}

int arch_vmctl(MESSAGE* m, struct proc* p)
{
    int request = m->VMCTL_REQUEST;

    switch (request) {
    case VMCTL_GETPDBR:
        m->VMCTL_PHYS_ADDR = (unsigned long)p->seg.cr3_phys;
        return 0;
    case VMCTL_SET_ADDRESS_SPACE:
        setcr3(p, (unsigned long)m->VMCTL_PHYS_ADDR, __va(m->VMCTL_PHYS_ADDR));
        return 0;
    case VMCTL_FLUSHTLB:
        reload_cr3();
        return 0;
    }

    return EINVAL;
}

void mm_suspend(struct proc* caller, endpoint_t target, void* laddr,
                size_t bytes, int write, int type)
{
    if (caller->endpoint == TASK_MM) {
        panic("mm_suspend: mm suspended target=%d laddr=%lx bytes=%d write=%d",
              target, laddr, bytes, write);
    }

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

static int check_mm_result(struct proc* caller)
{
    if (caller && (caller->flags & PF_RESUME_SYSCALL)) {
        return caller->mm_request.result;
    }

    return 0;
}

int _vir_copy(struct proc* caller, struct vir_addr* dest_addr,
              struct vir_addr* src_addr, size_t bytes, int check)
{
    struct vir_addr* vir_addrs[2];
    struct proc* procs[2];
    int retval;

    if (bytes < 0) return EINVAL;

    vir_addrs[_SRC_] = src_addr;
    vir_addrs[_DEST_] = dest_addr;

    int i;
    for (i = _SRC_; i <= _DEST_; i++) {
        endpoint_t proc_ep = vir_addrs[i]->proc_ep;

        procs[i] = proc_ep == NO_TASK ? NULL : endpt_proc(proc_ep);

        if (proc_ep != NO_TASK && procs[i] == NULL) return ESRCH;
    }

    if ((retval = check_mm_result(caller)) != OK) return retval;

    retval = la_la_copy(procs[_DEST_], vir_addrs[_DEST_]->addr, procs[_SRC_],
                        vir_addrs[_SRC_]->addr, bytes);

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
        } else {
            return EINVAL;
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
