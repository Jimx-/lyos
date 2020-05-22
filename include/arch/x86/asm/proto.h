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

#ifndef _ARCH_PROTO_H_
#define _ARCH_PROTO_H_

#define cmb() __asm__ __volatile__("" ::: "memory")

PUBLIC void x86_lgdt(u8* p_gdt);
PUBLIC void x86_lidt(u8* p_idt);
PUBLIC void x86_lldt(u32 ldt);
PUBLIC void x86_ltr(u32 tss);
PUBLIC void x86_load_kerncs();
PUBLIC void x86_load_ds(u32 ds);
PUBLIC void x86_load_es(u32 es);
PUBLIC void x86_load_fs(u32 fs);
PUBLIC void x86_load_gs(u32 gs);
PUBLIC void x86_load_ss(u32 ss);

PUBLIC u32 read_cr0();
PUBLIC void write_cr0(u32 cr0);
PUBLIC void write_cr3(u32 cr3);
PUBLIC u32 read_cr4();
PUBLIC void write_cr4(u32 cr4);

PUBLIC void arch_pause();

PUBLIC int init_8253_timer(int freq);
PUBLIC void stop_8253_timer();

PUBLIC void switch_k_stack(char* esp, void* cont);

PUBLIC reg_t read_ebp();

#ifndef __GNUC__
/* call a function to read the stack fram pointer (%ebp) */
#define get_stack_frame(__X) ((reg_t)read_ebp())
#else
/* read %ebp directly */
#define get_stack_frame(__X) ((reg_t)__builtin_frame_address(0))
#endif

PUBLIC int arch_init_proc(struct proc* p, void* sp, void* ip,
                          struct ps_strings* ps, char* name);
PUBLIC int arch_reset_proc(struct proc* p);
PUBLIC void arch_boot_proc(struct proc* p, struct boot_proc* bp);

PUBLIC struct proc* arch_switch_to_user();

PUBLIC int arch_get_kern_mapping(int index, caddr_t* addr, int* len,
                                 int* flags);
PUBLIC int arch_reply_kern_mapping(int index, void* vir_addr);
PUBLIC int arch_vmctl(MESSAGE* m, struct proc* p);

PUBLIC void sys_call_sysenter();

PUBLIC int syscall_int(int syscall_nr, MESSAGE* m);
PUBLIC int syscall_sysenter(int syscall_nr, MESSAGE* m);
PUBLIC int syscall_syscall(int syscall_nr, MESSAGE* m);

struct exception_frame {
    reg_t vec_no;   /* which interrupt vector was triggered */
    reg_t err_code; /* zero if no exception does not push err code */
    reg_t eip;
    reg_t cs;
    reg_t eflags;
};

PUBLIC void fninit();

PUBLIC void ia32_read_msr(u32 reg, u32* hi, u32* lo);
PUBLIC void ia32_write_msr(u32 reg, u32 hi, u32 lo);

PUBLIC void halt_cpu();

PUBLIC int init_local_timer(int freq);
PUBLIC int put_local_timer_handler(irq_handler_t handler);
PUBLIC void restart_local_timer();
PUBLIC void stop_local_timer();
void arch_stop_context(struct proc* p, u64 delta);
void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]);

PUBLIC void smp_commence();

PUBLIC void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end);
PUBLIC void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end,
                   kinfo_t* pk);
PUBLIC phys_bytes pg_alloc_pages(kinfo_t* pk, unsigned int nr_pages);
PUBLIC phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size);

PUBLIC void clear_memcache();

PUBLIC void arch_set_syscall_result(struct proc* p, int result);

PUBLIC void arch_setup_cpulocals(void);

#endif
