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

void out_byte(u16 port, u8 value);
u8 in_byte(u16 port);
void out_word(u16 port, u16 val);
u16 in_word(u16 port);
void out_long(u16 port, u32 val);
u32 in_long(u16 port);

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

unsigned long read_cr0();
void write_cr0(unsigned long cr0);
void write_cr3(unsigned long cr3);
unsigned long read_cr4();
void write_cr4(unsigned long cr4);

void load_tls(struct proc* p, unsigned int cpu);

void arch_init_irq(void);

void arch_pause();

void init_8259A();
void disable_8259A();

int init_8253_timer(void);

void switch_k_stack(char* esp, void* cont);

unsigned long read_ebp();

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

int syscall_int(int syscall_nr, MESSAGE* m);
int syscall_sysenter(int syscall_nr, MESSAGE* m);
int syscall_syscall(int syscall_nr, MESSAGE* m);

void fninit(void);
unsigned short fnstsw(void);
void fnstcw(unsigned short* cw);
void fnsave(void* state);
void fxsave(void* state);
void xsave(void* state, u32 hi, u32 lo);
int frstor(void* state);
int frstor_end(void*);
int fxrstor(void* state);
int fxrstor_end(void*);
int xrstor(void* state, u32 hi, u32 lo);
int xrstor_end(void*);
int frstor_fault(void*);
void clts(void);

void ia32_read_msr(u32 reg, u32* hi, u32* lo);
void ia32_write_msr(u32 reg, u32 hi, u32 lo);

void halt_cpu();

void arch_stop_context(struct proc* p, u64 delta);
void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]);

void smp_commence();

/* page.c */
int pg_identity_map(pde_t* pgd_page, phys_bytes pstart, phys_bytes pend,
                    unsigned long offset);
void pg_unmap_identity(pde_t* pgd);
void pg_mapkernel(pde_t* pgd);
void pg_load(pde_t* pgd);
void enable_paging();
void disable_paging();
unsigned long read_cr2();
unsigned long read_cr3();
void reload_cr3();
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
int sys_arch_prctl(MESSAGE* m, struct proc* p_proc);

static inline u64 xgetbv(u32 index)
{
    u32 eax, edx;

    asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return eax + ((u64)edx << 32);
}

static inline void xsetbv(u32 index, u64 value)
{
    u32 eax = value;
    u32 edx = value >> 32;

    asm volatile("xsetbv" ::"a"(eax), "d"(edx), "c"(index));
}

#endif
