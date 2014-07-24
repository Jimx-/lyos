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
PUBLIC void	disp_str(char * info);
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
PUBLIC void 	Init();
PUBLIC int  	get_ticks();
PUBLIC void 	panic(const char *fmt, ...);

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

/* kernel/block/scsi/scsi.c */
PUBLIC void 	task_scsi();
PUBLIC void 	init_scsi();
PUBLIC void 	scsi_handler(int irq);
PUBLIC void 	do_scsi_request();

/* kernel/block/pci/pci.c */
PUBLIC void 	task_pci();
PUBLIC void 	pci_init();

PUBLIC void 	task_inet();

PUBLIC void     task_devman();

/* keyboard.c */
PUBLIC void 	init_keyboard();
PUBLIC void 	kb_init(TTY * tty);
PUBLIC void 	keyboard_read(TTY* p_tty);

/* tty.c */
PUBLIC void 	task_tty();
PUBLIC void 	handle_events(TTY* tty);
PUBLIC void 	in_process(TTY* p_tty, u32 key);
PUBLIC void 	dump_tty_buf();	/* for debug only */

/* systask.c */
PUBLIC void 	task_sys();

/* fs/main.c */
/*PUBLIC void	task_fs();*/

/* fs/Lyos/main.c */
PUBLIC void	    task_fs();
PUBLIC void	    init_fs();
PUBLIC void 	mount_root();
PUBLIC int      rw_sector	(int io_type, int dev, u64 pos,
						int bytes, int proc_nr, void * buf);
PUBLIC struct inode *		get_inode	(int dev, int num);
PUBLIC void	    put_inode	(struct inode * pinode);
PUBLIC void	    sync_inode	(struct inode * p);
PUBLIC void 	read_super_block(int dev);
PUBLIC struct super_block *	get_super_block	(int dev);

/* fs/Lyos/misc.c */
PUBLIC int	     do_stat(MESSAGE * p);
PUBLIC int       do_fstat(MESSAGE * p);
PUBLIC int       do_access(MESSAGE * p);

/* fs/ext2/main.c */
PUBLIC void task_ext2_fs();

/* fs/initfs/main.c */
PUBLIC void task_initfs();

/* mm/main.c */
PUBLIC void	task_mm();

/* mm/alloc.c */
PUBLIC void	mem_init(int mem_start, int free_mem_size);
PUBLIC void vmem_init(int mem_start, int free_mem_size);
/* PUBLIC int	alloc_mem(int pid, int memsize); */
PUBLIC int	alloc_mem(int memsize);
PUBLIC int	free_mem(int base, int len);
PUBLIC int alloc_pages(int nr_pages);

PUBLIC int  alloc_vmem(int memsize);
PUBLIC int alloc_vmpages(int nr_pages);
PUBLIC int  free_vmem(int base, int len);

PUBLIC int map_page(struct page_directory * pgd, void * phys_addr, void * vir_addr);

/* console.c */
PUBLIC void	out_char(TTY* tty, char ch);
PUBLIC void 	scroll_screen(CONSOLE* p_con, int direction);
PUBLIC void 	select_console(int nr_console);
PUBLIC void 	init_screen(TTY* p_tty);
PUBLIC int  	is_current_console(CONSOLE* p_con);

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

/* fs/Lyos/open.c */
PUBLIC int	do_open(MESSAGE * p);
PUBLIC int	do_close(MESSAGE * p);
PUBLIC int	do_lseek(MESSAGE * p);
PUBLIC int	do_chroot(MESSAGE * p);
PUBLIC int 	do_mount(MESSAGE * p);
PUBLIC int 	do_umount(MESSAGE * p);
PUBLIC int 	do_mkdir(MESSAGE * p);

/* fs/Lyos/read_write.c */
PUBLIC int	do_rdwt(MESSAGE * p);

/* fs/Lyos/link.c */
PUBLIC int	do_unlink(MESSAGE * p);

/* mm/forkexit.c */
PUBLIC int	do_fork();
PUBLIC void	do_exit(int status);
PUBLIC void	do_wait();
PUBLIC int	do_kill();

/* sys.c */
PUBLIC int 	do_brk();

/* signal.h */
PUBLIC int 	do_sigaction();
PUBLIC int 	do_raise();
PUBLIC int 	do_alarm();
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

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */

/* system call */
PUBLIC	int	sendrec(int function, int src_dest, MESSAGE* p_msg);
PUBLIC	int	printx(char* str);
PUBLIC  int datacopy(MESSAGE * m);

#define	phys_copy	memcpy
#define	phys_set	memset

PUBLIC  int     printl(const char *fmt, ...);
