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

void x86_lgdt(u8* p_gdt);
void x86_lidt(u8* p_idt);
void x86_lldt(u32 ldt);
void x86_ltr(u32 tss);
void x86_load_kerncs();
void x86_load_ds(u32 ds);
void x86_load_es(u32 es);
void x86_load_fs(u32 fs);
void x86_load_gs(u32 gs);
void x86_load_ss(u32 ss);

u32 read_cr0();
void write_cr0(u32 cr0);
void write_cr3(u32 cr3);
u32 read_cr4();
void write_cr4(u32 cr4);

void load_tls(struct proc* p, unsigned int cpu);

void arch_pause();

int init_8253_timer(int freq);
void stop_8253_timer();

void switch_k_stack(char* esp, void* cont);

reg_t read_ebp();

#ifndef __GNUC__
/* call a function to read the stack fram pointer (%ebp) */
#define get_stack_frame(__X) ((reg_t)read_ebp())
#else
/* read %ebp directly */
#define get_stack_frame(__X) ((reg_t)__builtin_frame_address(0))
#endif

int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name);
int arch_fork_proc(struct proc* p, struct proc* parent, int flags, void* newsp,
                   void* tls);
int arch_reset_proc(struct proc* p);
void arch_boot_proc(struct proc* p, struct boot_proc* bp);

struct proc* arch_switch_to_user();

int arch_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags);
int arch_reply_kern_mapping(int index, void* vir_addr);
int arch_vmctl(MESSAGE* m, struct proc* p);

void sys_call_sysenter();

int syscall_int(int syscall_nr, MESSAGE* m);
int syscall_sysenter(int syscall_nr, MESSAGE* m);
int syscall_syscall(int syscall_nr, MESSAGE* m);

void fninit(void);
unsigned short fnstsw(void);
void fnstcw(unsigned short* cw);
void fnsave(void* state);
void fxsave(void* state);
int frstor(void* state);
int frstor_end(void*);
int fxrstor(void* state);
int fxrstor_end(void*);
int frstor_fault(void*);
void clts(void);

void ia32_read_msr(u32 reg, u32* hi, u32* lo);
void ia32_write_msr(u32 reg, u32 hi, u32 lo);

void halt_cpu();

int init_local_timer(int freq);
void setup_local_timer_one_shot(void);
void setup_local_timer_periodic(void);
int put_local_timer_handler(irq_handler_t handler);
void restart_local_timer();
void stop_local_timer();
void arch_stop_context(struct proc* p, u64 delta);
void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]);

void smp_commence();

void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end);
void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t* pk);
phys_bytes pg_alloc_pages(kinfo_t* pk, unsigned int nr_pages);
phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size);

void clear_memcache();

void arch_set_syscall_result(struct proc* p, int result);

void arch_setup_cpulocals(void);

int do_set_thread_area(struct proc* p, int idx, void* u_info, int can_allocate,
                       struct proc* caller);
int sys_set_thread_area(MESSAGE* m, struct proc* p_proc);

#endif
