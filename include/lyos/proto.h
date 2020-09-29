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

#ifndef _LYOS_PROTO_H_
#define _LYOS_PROTO_H_

#include <lyos/types.h>

/* kliba.asm */
void out_byte(u16 port, u8 value);
u8 in_byte(u16 port);
void out_word(u16 port, u16 val);
u16 in_word(u16 port);
void out_long(u16 port, u32 val);
u32 in_long(u16 port);
void disable_int();
void enable_int();
void port_read(u16 port, void* buf, int n);
void port_write(u16 port, void* buf, int n);

void read_tsc(unsigned long* high, unsigned long* low);
void read_tsc_64(u64* v);

void switch_address_space(struct proc* p);

/* klib.c */
void delay(int time);
void disp_int(int input);

/* kernel.asm */
void restore_user_context(struct proc* p);
void restore_user_context_int(struct proc* p);
void restore_user_context_sysenter(struct proc* p);
void restore_user_context_syscall(struct proc* p);

/* main.c */
void finish_bsp_booting();
void panic(const char* fmt, ...);

/* system.c */
void identify_cpu();

/* fpu.c */
void fpu_init(void);
void enable_fpu_exception(void);
void disable_fpu_exception(void);
void save_local_fpu(struct proc* p, int retain);
int restore_fpu(struct proc* p);

/* smp.c */
void smp_init();

/* memory.c */
void init_memory();

/* i8259.c */
void init_8259A();

/* interrupt.c */
void init_irq();
void irq_handle(int irq);
void put_irq_handler(int irq, irq_hook_t* hook, irq_handler_t handler);
void rm_irq_handler(irq_hook_t* hook);
int disable_irq(irq_hook_t* hook);
void enable_irq(irq_hook_t* hook);

/* clock.c */
int arch_init_time();
int init_time();
int init_bsp_timer(int freq);
int init_ap_timer(int freq);
void stop_context(struct proc* p);
void set_sys_timer(struct timer_list* timer);
void reset_sys_timer(struct timer_list* timer);

/* proc.c */
void init_proc();
void switch_to_user();
int verify_endpt(endpoint_t ep, int* proc_nr);
struct proc* endpt_proc(endpoint_t ep);
void* va2la(endpoint_t ep, void* va);
void* la2pa(endpoint_t ep, void* la);
void* va2pa(endpoint_t ep, void* va);
int msg_send(struct proc* p_to_send, int dest, MESSAGE* m, int flags);
int msg_notify(struct proc* p_to_send, endpoint_t dest);
void reset_msg(MESSAGE* p);
void dump_msg(const char* title, MESSAGE* m);
void dump_proc(struct proc* p);
int send_recv(int function, int src_dest, MESSAGE* msg);
void enqueue_proc(struct proc* p);
void dequeue_proc(struct proc* p);
void copr_not_available_handler(void);
void release_fpu(struct proc* p);

/* sched.c */
void init_sched();

/* profile.c */
#if CONFIG_PROFILING
void init_profile_clock(u32 freq);
void stop_profile_clock();
void nmi_profile_handler(int in_kernel, void* pc);
#endif

/* watchdog.c */
int init_profile_nmi(unsigned int freq);
void stop_profile_nmi(void);
void nmi_watchdog_handler(int in_kernel, void* pc);

/* direct_tty.c */
void direct_put_str(const char* str);
int direct_print(const char* fmt, ...);
void direct_cls();

/* system.c */
void init_system();
int resume_sys_call(struct proc* p);
int set_priv(struct proc* p, int id);
void ksig_proc(endpoint_t ep, int signo);

/* lib/misc.c */
u32 now();

endpoint_t get_endpoint();

int data_copy(endpoint_t dest_ep, void* dest_addr, endpoint_t src_ep,
              void* src_addr, int len);
int _vir_copy(struct proc* caller, struct vir_addr* dest_addr,
              struct vir_addr* src_addr, size_t bytes, int check);
int _data_vir_copy(struct proc* caller, endpoint_t dest_ep, void* dest_addr,
                   endpoint_t src_ep, void* src_addr, int len, int check);
#define vir_copy(dest, src, bytes) _vir_copy(NULL, dest, src, bytes, 0)
#define vir_copy_check(caller, dest, src, bytes) \
    _vir_copy(caller, dest, src, bytes, 1)
#define data_vir_copy(dest_ep, dest_addr, src_ep, src_addr, len) \
    _data_vir_copy(NULL, dest_ep, dest_addr, src_ep, src_addr, len, 0)
#define data_vir_copy_check(caller, dest_ep, dest_addr, src_ep, src_addr, len) \
    _data_vir_copy(caller, dest_ep, dest_addr, src_ep, src_addr, len, 1)

int service_up(const char* name, char* argv[], char* const envp[]);

/* System calls */
int sys_sendrec(MESSAGE* m, struct proc* p);
int sys_printx(MESSAGE* m, struct proc* p_proc);
int sys_datacopy(MESSAGE* m, struct proc* p_proc);
int sys_privctl(MESSAGE* m, struct proc* p);
int sys_getinfo(MESSAGE* m, struct proc* p_proc);
int sys_vmctl(MESSAGE* m, struct proc* p_proc);
int sys_umap(MESSAGE* m, struct proc* p);
int sys_portio(MESSAGE* m, struct proc* p_proc);
int sys_vportio(MESSAGE* m, struct proc* p_proc);
int sys_sportio(MESSAGE* m, struct proc* p_proc);
int sys_irqctl(MESSAGE* m, struct proc* p_proc);
int sys_fork(MESSAGE* m, struct proc* p_proc);
int sys_clear(MESSAGE* m, struct proc* p_proc);
int sys_exec(MESSAGE* m, struct proc* p_proc);
int sys_sigsend(MESSAGE* m, struct proc* p);
int sys_sigreturn(MESSAGE* m, struct proc* p);
int sys_kill(MESSAGE* m, struct proc* p_proc);
int sys_getksig(MESSAGE* m, struct proc* p_proc);
int sys_endksig(MESSAGE* m, struct proc* p_proc);
int sys_times(MESSAGE* m, struct proc* p_proc);
int sys_trace(MESSAGE* m, struct proc* p_proc);
int sys_alarm(MESSAGE* m, struct proc* p_proc);
int sys_kprofile(MESSAGE* m, struct proc* p_proc);
int sys_setgrant(MESSAGE* m, struct proc* p_proc);
int sys_safecopyfrom(MESSAGE* msg, struct proc* p_proc);
int sys_safecopyto(MESSAGE* msg, struct proc* p_proc);

/* syscall.asm */
void sys_call(); /* int_handler */

/* system call */
int sendrec(int function, int src_dest, MESSAGE* p_msg);
int printx(char* s);
int getinfo(int request, void* buf);
int vmctl(int request, endpoint_t who);

phys_bytes phys_copy(phys_bytes dest, phys_bytes src, phys_bytes len);
int copy_user_message(MESSAGE* dest, MESSAGE* src);
#define phys_set memset

int printk(const char* fmt, ...);
int printl(const char* fmt, ...);
int get_sysinfo(struct sysinfo** sysinfo);
int get_kinfo(kinfo_t* kinfo);

int verify_grant(endpoint_t granter, endpoint_t grantee, mgrant_id_t grant,
                 size_t bytes, int access, vir_bytes offset, vir_bytes* voffset,
                 endpoint_t* new_granter);

#endif
