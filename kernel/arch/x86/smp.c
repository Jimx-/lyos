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
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "acpi.h"
#include "apic.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <lyos/cpulocals.h>
#include <lyos/smp.h>

/* trampoline parameters */
extern volatile u32 __ap_id, __ap_pgd;
extern volatile u8 __ap_gdt[6], __ap_idt[6]; /* 0~15:Limit  16~47:Base */
extern u32 __ap_gdt_table, __ap_idt_table;
extern void* __trampoline_end;
phys_bytes trampoline_base;

PRIVATE u8 apicid2cpuid[255];
PUBLIC u8 cpuid2apicid[CONFIG_SMP_MAX_CPUS];

PRIVATE u32 ap_ready;

PRIVATE int smp_commenced = 0;

PRIVATE int discover_cpus();
PRIVATE void init_tss_all();
PRIVATE void smp_start_aps();

PUBLIC void trampoline();

#define AP_LIN_ADDR(addr) \
    (phys_bytes)((u32)addr - (u32)&trampoline + trampoline_base)

PRIVATE void copy_trampoline()
{
    phys_bytes tramp_size, tramp_start = (phys_bytes)&trampoline;

    tramp_size = (phys_bytes)&__trampoline_end - tramp_start;

    trampoline_base = pg_alloc_lowest(&kinfo, tramp_size);

    /* copy GDT and IDT */
    memcpy(&__ap_gdt_table, gdt, sizeof(gdt));
    memcpy(&__ap_idt_table, idt, sizeof(idt));

    /* Set up descriptors */
    u16* gdt_limit = (u16*)(&__ap_gdt[0]);
    u32* gdt_base = (u32*)(&__ap_gdt[2]);
    *gdt_limit = GDT_SIZE * sizeof(struct descriptor) - 1;
    *gdt_base = AP_LIN_ADDR(&__ap_gdt_table);

    u16* idt_limit = (u16*)(&__ap_idt[0]);
    u32* idt_base = (u32*)(&__ap_idt[2]);
    *idt_limit = GDT_SIZE * sizeof(struct descriptor) - 1;
    *idt_base = AP_LIN_ADDR(&__ap_idt_table);

    memcpy((void*)trampoline_base, trampoline, tramp_size);
}

PUBLIC void smp_init()
{
    ncpus = 0;

    if (!discover_cpus()) {
        ncpus = 1;
    }

    machine.cpu_count = ncpus;

    init_tss_all(bsp_cpu_id, (u32)get_k_stack_top(bsp_cpu_id));

    lapic_addr = (void*)LOCAL_APIC_DEF_ADDR;

    bsp_lapic_id = apicid();
    bsp_cpu_id = apicid2cpuid[bsp_lapic_id];

    if (!lapic_enable(bsp_cpu_id)) {
        panic("unable to initialize bsp lapic");
    }

    if (!detect_ioapics()) {
        panic("no ioapic detected");
    }

    ioapic_enable();

    apic_init_idt(0);
    reload_idt();

    switch_k_stack((char*)get_k_stack_top(bsp_cpu_id) - X86_STACK_TOP_RESERVED,
                   smp_start_aps);
}

PRIVATE void init_tss_all()
{
    unsigned cpu;

    for (cpu = 0; cpu < ncpus; cpu++) {
        init_tss(cpu, (u32)get_k_stack_top(cpu));
    }
}

PRIVATE int discover_cpus()
{
    struct acpi_madt_lapic* cpu;

    while (ncpus < CONFIG_SMP_MAX_CPUS && (cpu = acpi_get_lapic_next())) {
        apicid2cpuid[cpu->apic_id] = ncpus;
        cpuid2apicid[ncpus] = cpu->apic_id;
        printk("CPU %3d local APIC id 0x%03x %s\n", ncpus, cpu->apic_id,
               ncpus == 0 ? "(bsp)" : "");
        ncpus++;
    }

    return ncpus;
}

PRIVATE void smp_start_aps()
{
    u32 reset_vector;

    memcpy(&reset_vector, (void*)0x467, sizeof(u32));

    /* set CMOS system shutdown mode */
    out_byte(CLK_ELE, 0xF);
    out_byte(CLK_IO, 0xA);

    __ap_pgd = get_cpulocal_var(pt_proc)->seg.cr3_phys;

    /* set up AP boot code */
    copy_trampoline();

    phys_bytes __ap_id_phys = AP_LIN_ADDR(&__ap_id);
    memcpy((void*)0x467, &trampoline_base, sizeof(u32));

    int i;
    for (i = 0; i < ncpus; i++) {
        ap_ready = -1;
        if (apicid() == cpuid2apicid[i] && bsp_lapic_id == apicid()) continue;

        __ap_id = booting_cpu = i;
        memcpy((void*)__ap_id_phys, (void*)&__ap_id, sizeof(u32));

        /* INIT-SIPI-SIPI sequence */
        cmb();
        if (apic_send_init_ipi(i, trampoline_base) ||
            apic_send_startup_ipi(i, trampoline_base)) {
            printk("smp: cannot boot CPU %d\n", i);
            continue;
        }

        lapic_set_timer_one_shot(5000000);

        while (lapic_read(LAPIC_TIMER_CCR)) {
            if (ap_ready == i) {
                set_cpu_flag(i, CPU_IS_READY);
                lapic_set_timer_one_shot(0);
                break;
            }
        }
        if (ap_ready == -1) {
            printk("smp: CPU %d didn't boot\n", i);
        }
    }

    memcpy((void*)0x467, &reset_vector, sizeof(u32));
    out_byte(CLK_ELE, 0xF);
    out_byte(CLK_IO, 0);

    init_tss(bsp_cpu_id, (u32)get_k_stack_top(bsp_cpu_id));

    finish_bsp_booting();
}

PRIVATE void ap_finish_booting()
{
    ap_ready = cpuid;

    init_tss(cpuid, (u32)get_k_stack_top(cpuid));

    printk("smp: CPU %d is up\n", cpuid);

    identify_cpu();

    get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);

    lapic_enable(cpuid);
    fpu_init();

    if (init_ap_timer(system_hz) != 0)
        panic("smp: cannot init timer for CPU %d", cpuid);

    ap_finished_booting();

    /* wait for MM is ready */
    while (!smp_commenced)
        ;
    /* flush TLB */
    switch_address_space(proc_addr(TASK_MM));

    switch_to_user();
}

PUBLIC void smp_boot_ap()
{
    switch_k_stack((char*)get_k_stack_top(__ap_id) - X86_STACK_TOP_RESERVED,
                   ap_finish_booting);
}

PUBLIC void smp_commence() { smp_commenced = 1; }
