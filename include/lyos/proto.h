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
PUBLIC void	disable_irq(int irq);
PUBLIC void	enable_irq(int irq);
PUBLIC void	disable_int();
PUBLIC void	enable_int();
PUBLIC void	port_read(u16 port, void* buf, int n);
PUBLIC void	port_write(u16 port, void* buf, int n);
PUBLIC void	glitter(int row, int col);

PUBLIC void switch_address_space(struct proc * p);

/* klib.c */
PUBLIC void	delay(int time);
PUBLIC void	disp_int(int input);
PUBLIC char *	itoa(char * str, int num);

/* kernel.asm */
PUBLIC void     restore_user_context(struct proc * p);
PUBLIC void 	restore_user_context_int(struct proc * p);

/* main.c */
PUBLIC void     finish_bsp_booting();
PUBLIC int  	get_ticks();
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
PUBLIC void     irq_handle(int irq);
PUBLIC void     put_irq_handler(int irq, irq_hook_t * hook, irq_handler_t handler);

/* clock.c */
PUBLIC void 	init_clock();
PUBLIC void 	milli_delay(int milli_sec);

/* kernel/block/hd.c */
PUBLIC void 	task_hd();
	
/* kernel/block/fd.c */
PUBLIC void 	task_fd();
PUBLIC void 	init_fd();
PUBLIC void 	fd_handler(int irq);

/* kernel/block/rd.c */
PUBLIC void 	task_rd();

PUBLIC void 	task_inet();

PUBLIC void     task_devman();

/* keyboard.c */
PUBLIC void 	init_keyboard();

/* tty.c */
PUBLIC void 	task_tty();

/* systask.c */
PUBLIC void 	task_sys();

/* fs/lyos/main.c */
PUBLIC void	    task_fs();
PUBLIC void	    init_fs();

/* fs/ext2/main.c */
PUBLIC void task_ext2_fs();

/* fs/initfs/main.c */
PUBLIC void task_initfs();

/* mm/main.c */
PUBLIC void	task_mm();

/* servman/servman.c */
PUBLIC void task_servman();

/* proc.c */
PUBLIC  void    init_proc();
PUBLIC  void    switch_to_user();
PUBLIC	void*	va2la(int pid, void * va);
PUBLIC  void*   la2pa(int pid, void * la);
PUBLIC  void*   va2pa(int pid, void * va);
PUBLIC	int	    ldt_seg_linear(struct proc* p, int idx);
PUBLIC	void	reset_msg(MESSAGE* p);
PUBLIC	void	dump_msg(const char * title, MESSAGE* m);
PUBLIC	void	dump_proc(struct proc * p);
PUBLIC	int	    send_recv(int function, int src_dest, MESSAGE* msg);
PUBLIC  void	inform_int(int task_nr);
PUBLIC  void    inform_kernel_log(int task_nr);
PUBLIC  void    enqueue_proc(register struct proc * p);
PUBLIC  void    dequeue_proc(register struct proc * p);

/* direct_tty.c */
PUBLIC  int     direct_print(const char * fmt, ...);
PUBLIC  void    direct_cls();

/* lib/misc.c */
PUBLIC u32      now();

PUBLIC int      data_copy(endpoint_t dest_pid, void * dest_addr, 
    endpoint_t src_pid, void * src_addr, int len);
PUBLIC int      vir_copy(endpoint_t dest_pid, void * dest_addr,
                        endpoint_t src_pid, void * src_addr, int len);

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

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */

/* system call */
PUBLIC	int	sendrec(int function, int src_dest, MESSAGE* p_msg);
PUBLIC	int	printx(char * s);
PUBLIC  int privctl(int whom, int request, void * data);
PUBLIC  int getinfo(int request, void* buf);
PUBLIC  int vmctl(int request, endpoint_t who);

#define	phys_copy	memcpy
#define	phys_set	memset

PUBLIC  int     printk(const char *fmt, ...);
PUBLIC  int     printl(const char *fmt, ...);
PUBLIC  int     get_sysinfo(struct sysinfo ** sysinfo);
PUBLIC  int     get_kinfo(kinfo_t * kinfo);
