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
PUBLIC void 	out_word(u16 port, u16 val);
PUBLIC u16  	in_word(u16 port);
PUBLIC void 	out_long(u16 port, u32 val);
PUBLIC u32  	in_long(u16 port);
PUBLIC void disp_char(const char c);
PUBLIC int	disp_str(const char *fmt, ...);
PUBLIC void	disp_color_str(char * info, int color);
PUBLIC void	disable_irq(int irq);
PUBLIC void	enable_irq(int irq);
PUBLIC void	disable_int();
PUBLIC void	enable_int();
PUBLIC void	port_read(u16 port, void* buf, int n);
PUBLIC void	port_write(u16 port, void* buf, int n);
PUBLIC void	glitter(int row, int col);

/* klib.c */
PUBLIC void	delay(int time);
PUBLIC void	disp_int(int input);
PUBLIC char *	itoa(char * str, int num);

/* kernel.asm */
PUBLIC void 	restart();

/* main.c */
PUBLIC void     finish_bsp_booting();
PUBLIC void 	Init();
PUBLIC int  	get_ticks();
PUBLIC void 	panic(const char *fmt, ...);

/* smp.c */
PUBLIC void     smp_init();

/* i8259.c */
PUBLIC void 	init_8259A();
PUBLIC void 	put_irq_handler(int irq, irq_handler handler);
PUBLIC void 	spurious_irq(int irq);

/* clock.c */
PUBLIC void 	clock_handler(int irq);
PUBLIC void 	init_clock();
PUBLIC void 	milli_delay(int milli_sec);

/* kernel/block/hd.c */
PUBLIC void 	task_hd();
PUBLIC void 	hd_handler(int irq);
	
/* kernel/block/fd.c */
PUBLIC void 	task_fd();
PUBLIC void 	init_fd();
PUBLIC void 	fd_handler(int irq);

/* kernel/block/rd.c */
PUBLIC void 	task_rd();

/* kernel/block/pci/pci.c */
PUBLIC void 	task_pci();
PUBLIC void 	pci_init();

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
PUBLIC	void	schedule();
PUBLIC	void*	va2la(int pid, void * va);
PUBLIC  void*   la2pa(int pid, void * la);
PUBLIC  void*   va2pa(int pid, void * va);
PUBLIC	int	    ldt_seg_linear(struct proc* p, int idx);
PUBLIC	void	reset_msg(MESSAGE* p);
PUBLIC	void	dump_msg(const char * title, MESSAGE* m);
PUBLIC	void	dump_proc(struct proc * p);
PUBLIC	int	    send_recv(int function, int src_dest, MESSAGE* msg);
PUBLIC  void	inform_int(int task_nr);

/* lib/misc.c */
PUBLIC void 	spin(char * func_name);
PUBLIC u32      now();

PUBLIC int  data_copy(endpoint_t dest_pid, int dest_seg, void * dest_addr, 
    endpoint_t src_pid, int src_seg, void * src_addr, int len);
PUBLIC int  vir_copy(endpoint_t dest_pid, int dest_seg, void * dest_addr,
                        endpoint_t src_pid, int src_seg, void * src_addr, int len);

PUBLIC int service_up(const char *name, char * argv[], char * const envp[]);

/* proc.c */
PUBLIC	int	sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p);
PUBLIC	int	sys_printx(int _unused1, int _unused2, char* s, struct proc * p_proc);
PUBLIC  int sys_datacopy(int _unused1, int _unused2, MESSAGE * m, struct proc * p_proc);
PUBLIC  int sys_privctl(int whom, int request, void * data, struct proc* p);

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */

/* system call */
PUBLIC	int	sendrec(int function, int src_dest, MESSAGE* p_msg);
PUBLIC	int	printx(char* str);
PUBLIC  int datacopy(MESSAGE * m);
PUBLIC  int privctl(int whom, int request, void * data);

#define	phys_copy	memcpy
#define	phys_set	memset

PUBLIC  int     printl(const char *fmt, ...);
