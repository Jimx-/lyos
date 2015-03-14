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

/* kliba.asm */
PUBLIC void	out_byte(u16 port, u8 value);
PUBLIC u8	in_byte(u16 port);
PUBLIC void out_word(u16 port, u16 val);
PUBLIC u16  in_word(u16 port);
PUBLIC void out_long(u16 port, u32 val);
PUBLIC u32  in_long(u16 port);
PUBLIC void	disable_int();
PUBLIC void	enable_int();
PUBLIC void	port_read(u16 port, void* buf, int n);
PUBLIC void	port_write(u16 port, void* buf, int n);

PUBLIC void read_tsc(unsigned long *high, unsigned long *low);
PUBLIC void read_tsc_64(u64 * v);

PUBLIC void switch_address_space(struct proc * p);

/* klib.c */
PUBLIC void	delay(int time);
PUBLIC void	disp_int(int input);
PUBLIC char *	itoa(char * str, int num);

/* kernel.asm */
PUBLIC void     restore_user_context(struct proc * p);
PUBLIC void 	restore_user_context_int(struct proc * p);
PUBLIC void     restore_user_context_sysenter(struct proc * p);

/* main.c */
PUBLIC void     finish_bsp_booting();
PUBLIC void 	panic(const char *fmt, ...);

/* system.c */
PUBLIC void     fpu_init();

/* smp.c */
PUBLIC void     smp_init();

/* memory.c */
PUBLIC void     init_memory();

/* i8259.c */
PUBLIC void 	init_8259A();

/* interrupt.c */
PUBLIC void     init_irq();
PUBLIC void     irq_handle(int irq);
PUBLIC void     put_irq_handler(int irq, irq_hook_t * hook, irq_handler_t handler);
PUBLIC int      disable_irq(irq_hook_t * hook);
PUBLIC void     enable_irq(irq_hook_t * hook);

/* clock.c */
PUBLIC int      arch_init_time();
PUBLIC int      init_time();
PUBLIC int   	init_bsp_timer(int freq);
PUBLIC int      init_ap_timer(int freq);
PUBLIC void     stop_context(struct proc * p);

/* proc.c */
PUBLIC  void    init_proc();
PUBLIC  void    switch_to_user();
PUBLIC  int     verify_endpt(endpoint_t ep, int * proc_nr);
PUBLIC  struct proc * endpt_proc(endpoint_t ep);
PUBLIC	void*	va2la(endpoint_t ep, void * va);
PUBLIC  void*   la2pa(endpoint_t ep, void * la);
PUBLIC  void*   va2pa(endpoint_t ep, void * va);
PUBLIC  int     msg_send(struct proc* p_to_send, int dest, MESSAGE* m, int flags);
PUBLIC  int     msg_notify(struct proc * p_to_send, endpoint_t dest);
PUBLIC	void	reset_msg(MESSAGE* p);
PUBLIC	void	dump_msg(const char * title, MESSAGE* m);
PUBLIC	void	dump_proc(struct proc * p);
PUBLIC	int	    send_recv(int function, int src_dest, MESSAGE* msg);
PUBLIC  void    enqueue_proc(struct proc * p);
PUBLIC  void    dequeue_proc(struct proc * p);

/* sched.c */
PUBLIC  void    init_sched();

/* direct_tty.c */
PUBLIC  void    direct_put_str(const char * str);
PUBLIC  int     direct_print(const char * fmt, ...);
PUBLIC  void    direct_cls();

/* system.c */
PUBLIC  int     set_priv(struct proc * p, int id);

/* lib/misc.c */
PUBLIC u32      now();

PUBLIC endpoint_t get_endpoint();

PUBLIC int      data_copy(endpoint_t dest_ep, void * dest_addr, 
    endpoint_t src_ep, void * src_addr, int len);
PUBLIC int      vir_copy(endpoint_t dest_ep, void * dest_addr,
                        endpoint_t src_ep, void * src_addr, int len);

PUBLIC int      service_up(const char *name, char * argv[], char * const envp[]);

/* proc.c */
PUBLIC  int sys_sendrec(MESSAGE* m, struct proc* p);
PUBLIC	int	sys_printx(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_datacopy(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_privctl(MESSAGE * m, struct proc* p);
PUBLIC  int sys_getinfo(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_vmctl(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_umap(MESSAGE * m, struct proc* p);
PUBLIC  int sys_portio(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_vportio(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_sportio(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_irqctl(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_fork(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_clear(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_exec(MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_sigsend(MESSAGE * m, struct proc* p);
PUBLIC  int sys_sigreturn(MESSAGE * m, struct proc* p);

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */

/* system call */
PUBLIC	int	sendrec(int function, int src_dest, MESSAGE* p_msg);
PUBLIC	int	printx(char * s);
PUBLIC  int privctl(int whom, int request, void * data);
PUBLIC  int getinfo(int request, void* buf);
PUBLIC  int vmctl(int request, endpoint_t who);

PUBLIC  phys_bytes phys_copy(phys_bytes dest, phys_bytes src, phys_bytes len);
#define	phys_set	memset

PUBLIC  int     printk(const char *fmt, ...);
PUBLIC  int     printl(const char *fmt, ...);
PUBLIC  int     get_sysinfo(struct sysinfo ** sysinfo);
PUBLIC  int     get_kinfo(kinfo_t * kinfo);
