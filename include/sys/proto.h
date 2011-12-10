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
PUBLIC void	disp_str(char * info);
PUBLIC void	disp_color_str(char * info, int color);
PUBLIC void	disable_irq(int irq);
PUBLIC void	enable_irq(int irq);
PUBLIC void	disable_int();
PUBLIC void	enable_int();
PUBLIC void	port_read(u16 port, void* buf, int n);
PUBLIC void	port_write(u16 port, void* buf, int n);
PUBLIC void	glitter(int row, int col);

/* string.asm */
PUBLIC char*	strcpy(char* dst, const char* src);

/* reboot.asm */
PUBLIC void hard_reboot(void);

/* protect.c */
PUBLIC void	init_prot();
PUBLIC u32	seg2linear(u16 seg);
PUBLIC void	init_desc(struct descriptor * p_desc,
			  u32 base, u32 limit, u16 attribute);

/* klib.c */
PUBLIC void	get_boot_params(struct boot_params * pbp);
PUBLIC int	get_kernel_map(unsigned int * b, unsigned int * l);
PUBLIC void	delay(int time);
PUBLIC void	disp_int(int input);
PUBLIC char *	itoa(char * str, int num);

/* kernel.asm */
PUBLIC void restart();

/* main.c */
PUBLIC void Init();
PUBLIC int  get_ticks();
PUBLIC void TestA();
PUBLIC void TestB();
PUBLIC void TestC();
PUBLIC void panic(const char *fmt, ...);

/* i8259.c */
PUBLIC void init_8259A();
PUBLIC void put_irq_handler(int irq, irq_handler handler);
PUBLIC void spurious_irq(int irq);

/* clock.c */
PUBLIC void clock_handler(int irq);
PUBLIC void init_clock();
PUBLIC void milli_delay(int milli_sec);

/* kernel/blk_dev/rw_blk.c */
PUBLIC void	add_request(dev_t dev, MESSAGE * m);

/* kernel/blk_dev/hd.c */
PUBLIC void task_hd();
PUBLIC void do_hd_request();
PUBLIC void hd_handler(int irq);

/* kernel/blk_dev/fd.c */
PUBLIC void task_fd();
PUBLIC void init_fd();
PUBLIC void fd_handler(int irq);

/* kernel/blk_dev/rd.c */
PUBLIC void task_rd();
PUBLIC void init_rd();
PUBLIC void rd_load_image(dev_t dev, int offset);

/* kernel/blk_dev/scsi/scsi.c */
PUBLIC void task_scsi();
PUBLIC void init_scsi();
PUBLIC void scsi_handler(int irq);
PUBLIC void do_scsi_request();

/* kernel/blk_dev/pci/pci.c */
PUBLIC void task_pci();
PUBLIC void pci_init();

PUBLIC void task_inet();

/* keyboard.c */
PUBLIC void init_keyboard();
PUBLIC void keyboard_read(TTY* p_tty);

/* tty.c */
PUBLIC void task_tty();
PUBLIC void in_process(TTY* p_tty, u32 key);
PUBLIC void dump_tty_buf();	/* for debug only */

/* systask.c */
PUBLIC void task_sys();

/* fs/main.c */
/*PUBLIC void			task_fs();*/

/* fs/Lyos/main.c */
PUBLIC void			task_fs();
PUBLIC void			init_fs();
PUBLIC void 			mount_root();
PUBLIC int			rw_sector	(int io_type, int dev, u64 pos,
						int bytes, int proc_nr, void * buf);
PUBLIC struct inode *		get_inode	(int dev, int num);
PUBLIC void			put_inode	(struct inode * pinode);
PUBLIC void			sync_inode	(struct inode * p);
PUBLIC void 			read_super_block(int dev);
PUBLIC struct super_block *	get_super_block	(int dev);

PUBLIC void			task_lyos_fs	();

/* fs/Lyos/buffer.c */
PUBLIC void 		bread		(struct buffer_head * bh);
PUBLIC char * 		find_buffer	(int dev, u64 pos, int cnt);
PUBLIC char * 		get_buffer	(int dev, u64 pos, int cnt);
PUBLIC void 		sync_buffer	();
PUBLIC void 		do_sync		();
PUBLIC void 		free_buffer	();
PUBLIC void		init_buffer	();

/* fs/Lyos/misc.c */
PUBLIC int			do_stat(MESSAGE * p);
PUBLIC int			strip_path	(char * filename, const char * pathname,
					struct inode** ppinode);
PUBLIC int			search_file(char * path);

/* fs/Lyos/disklog.c */
PUBLIC int			disklog(char * logstr); /* for debug */
PUBLIC void			dump_fd_graph(const char * fmt, ...);

/* mm/main.c */
PUBLIC void			task_mm();
PUBLIC int			alloc_mem(int pid, int memsize);
PUBLIC int			free_mem(int pid);

/* console.c */
PUBLIC void 		out_char(CONSOLE* p_con, char ch);
PUBLIC void 		scroll_screen(CONSOLE* p_con, int direction);
PUBLIC void 		select_console(int nr_console);
PUBLIC void 		init_screen(TTY* p_tty);
PUBLIC int  		is_current_console(CONSOLE* p_con);

/* proc.c */
PUBLIC	void	schedule();
PUBLIC	void*	va2la(int pid, void* va);
PUBLIC	int	ldt_seg_linear(struct proc* p, int idx);
PUBLIC	void	reset_msg(MESSAGE* p);
PUBLIC	void	dump_msg(const char * title, MESSAGE* m);
PUBLIC	void	dump_proc(struct proc * p);
PUBLIC	int	send_recv(int function, int src_dest, MESSAGE* msg);
PUBLIC void	inform_int(int task_nr);

/* fs/Lyos/open.c */
PUBLIC int		do_open(MESSAGE * p);
PUBLIC int		do_close(MESSAGE * p);
PUBLIC int		do_lseek(MESSAGE * p);
PUBLIC int		do_chdir(MESSAGE * p);
PUBLIC int		do_chroot(MESSAGE * p);
PUBLIC int 		do_mount(MESSAGE * p);
PUBLIC int 		do_umount(MESSAGE * p);
PUBLIC int 		do_mkdir(MESSAGE * p);

/* fs/Lyos/read_write.c */
PUBLIC int		do_rdwt(MESSAGE * p);

/* fs/Lyos/link.c */
PUBLIC int		do_unlink(MESSAGE * p);

/* fs/Lyos/misc.c */
PUBLIC int		do_stat(MESSAGE * p);

/* fs/Lyos/disklog.c */
PUBLIC int		do_disklog();

/* fs/Lyos/namei.c */
PUBLIC int namei(const char * path);
PUBLIC int 			   find_entry	(struct inode * dir_inode, 
									const char * file);

/* mm/forkexit.c */
PUBLIC int		do_fork();
PUBLIC void		do_exit(int status);
PUBLIC void		do_wait();
PUBLIC int		do_kill();

/* mm/exec.c */
PUBLIC int		do_exec();

/* sys.c */
PUBLIC int 		do_ftime();
PUBLIC int 		do_mknod();
PUBLIC int 		do_break();
PUBLIC int 		do_ptrace();
PUBLIC int 		do_stty();
PUBLIC int 		do_gtty();
PUBLIC int 		do_rename();
PUBLIC int 		do_prof();
PUBLIC int 		do_setgid();
PUBLIC int 		do_acct();
PUBLIC int 		do_phys();
PUBLIC int 		do_lock();
PUBLIC int		do_mpx();
PUBLIC int		do_ulimit();
PUBLIC int 		do_brk();
PUBLIC int 		do_setpgid();
PUBLIC int 		do_getpgrp();
PUBLIC int		do_setsid();
PUBLIC int		do_uname(int src, struct utsname * name);
PUBLIC int		do_umask();

/* proc.c*/
PUBLIC int 		do_setuid();
PUBLIC int		do_getuid();
PUBLIC int		do_geteuid();
PUBLIC int		do_getegid();
PUBLIC int		do_setgid();
PUBLIC int		do_getgid();
PUBLIC int		do_nice();

/* signal.h */
PUBLIC int do_sigaction();
PUBLIC int do_raise();
PUBLIC int do_alarm();


/* lib/misc.c */
PUBLIC void spin(char * func_name);

/* proc.c */
PUBLIC	int	sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p);
PUBLIC	int	sys_printx(int _unused1, int _unused2, char* s, struct proc * p_proc);
PUBLIC	int sys_reboot(int _unused1, int _unused2, int flags, struct proc* p);

/* syscall.asm */
PUBLIC  void    sys_call();             /* int_handler */

/* system call */
PUBLIC	int	sendrec(int function, int src_dest, MESSAGE* p_msg);
PUBLIC	int	printx(char* str);
PUBLIC	int reboot(int flags);
