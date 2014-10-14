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

#ifndef	_CONST_H_
#define _CONST_H_

/* max() & min() */
#define	max(a,b)	((a) > (b) ? (a) : (b))
#define	min(a,b)	((a) < (b) ? (a) : (b))

/* Color */
/*
 * e.g. MAKE_COLOR(BLUE, RED)
 *      MAKE_COLOR(BLACK, RED) | BRIGHT
 *      MAKE_COLOR(BLACK, RED) | BRIGHT | FLASH
 */
#define BLACK   0x0     /* 0000 */
#define WHITE   0x7     /* 0111 */
#define RED     0x4     /* 0100 */
#define GREEN   0x2     /* 0010 */
#define BLUE    0x1     /* 0001 */
#define FLASH   0x80    /* 1000 0000 */
#define BRIGHT  0x08    /* 0000 1000 */
#define	MAKE_COLOR(x,y)	((x<<4) | y) /* MAKE_COLOR(Background,Foreground) */

/* GDT 和 IDT 中描述符的个数 */
#define	GDT_SIZE	128
#define	IDT_SIZE	256

#define NR_LOCAL_SEGS	3
#define T	0
#define D	1
#define S	2

#define TRUE			1
#define FALSE			0

#define	STR_DEFAULT_LEN	1024
/* TTY */
#define NR_CONSOLES	3	/* consoles */

/* 8259A interrupt controller ports. */
#define	INT_M_CTL	0x20	/* I/O port for interrupt controller         <Master> */
#define	INT_M_CTLMASK	0x21	/* setting bits in this port disables ints   <Master> */
#define	INT_S_CTL	0xA0	/* I/O port for second interrupt controller  <Slave>  */
#define	INT_S_CTLMASK	0xA1	/* setting bits in this port disables ints   <Slave>  */

/* 8253/8254 PIT (Programmable Interval Timer) */
#define TIMER0         0x40 /* I/O port for timer channel 0 */
#define TIMER_MODE     0x43 /* I/O port for timer mode control */
#define RATE_GENERATOR 0x34 /* 00-11-010-0 :
			     * Counter0 - LSB then MSB - rate generator - binary
			     */
#define TIMER_FREQ     1193182L/* clock frequency for timer in PC and AT */
#define HZ             100  /* clock freq (software settable on IBM-PC) */

/* AT keyboard */
/* 8042 ports */
#define KB_DATA		0x60	/* I/O port for keyboard data
					Read : Read Output Buffer
					Write: Write Input Buffer(8042 Data&8048 Command) */
#define KB_CMD		0x64	/* I/O port for keyboard command
					Read : Read Status Register
					Write: Write Input Buffer(8042 Command) */
#define LED_CODE	0xED
#define KB_ACK		0xFA

/* VGA */
#define	CRTC_ADDR_REG	0x3D4	/* CRT Controller Registers - Addr Register */
#define	CRTC_DATA_REG	0x3D5	/* CRT Controller Registers - Data Register */
#define	START_ADDR_H	0xC	/* reg index of video mem start addr (MSB) */
#define	START_ADDR_L	0xD	/* reg index of video mem start addr (LSB) */
#define	CURSOR_H	0xE	/* reg index of cursor position (MSB) */
#define	CURSOR_L	0xF	/* reg index of cursor position (LSB) */
#define	V_MEM_BASE	(0xB8000	+ KERNEL_VMA) /* base of color video memory */
#define	V_MEM_SIZE	0x8000	/* 32K: B8000H -> BFFFFH */

/* CMOS */
#define CLK_ELE		0x70	/* CMOS RAM address register port (write only)
				 * Bit 7 = 1  NMI disable
				 *	   0  NMI enable
				 * Bits 6-0 = RAM address
				 */

#define CLK_IO		0x71	/* CMOS RAM data register port (read/write) */

#define  YEAR             9	/* Clock register addresses in CMOS RAM	*/
#define  MONTH            8
#define  DAY              7
#define  HOUR             4
#define  MINUTE           2
#define  SECOND           0
#define  CLK_STATUS    0x0B	/* Status register B: RTC configuration	*/
#define  CLK_HEALTH    0x0E	/* Diagnostic status: (should be set by Power
				 * On Self-Test [POST])
				 * Bit  7 = RTC lost power
				 *	6 = Checksum (for addr 0x10-0x2d) bad
				 *	5 = Config. Info. bad at POST
				 *	4 = Mem. size error at POST
				 *	3 = I/O board failed initialization
				 *	2 = CMOS time invalid
				 *    1-0 =    reserved
				 */

/* Hardware interrupts */
#define	NR_IRQ			16	/* Number of IRQs */
#define	CLOCK_IRQ		0
#define	KEYBOARD_IRQ	1
#define	CASCADE_IRQ		2	/* cascade enable for 2nd AT controller */
#define	ETHER_IRQ		3	/* default ethernet interrupt vector */
#define	SECONDARY_IRQ	3	/* RS232 interrupt vector for port 2 */
#define	RS232_IRQ		4	/* RS232 interrupt vector for port 1 */
#define	XT_WINI_IRQ		5	/* xt winchester */
#define	FLOPPY_IRQ		6	/* floppy disk */
#define	PRINTER_IRQ		7
#define PS_2_IRQ		12
#define	AT_WINI_IRQ		14	/* at winchester */

/* tasks */
/* 注意 TASK_XXX 的定义要与 global.c 中对应 */
#define INVALID_DRIVER	-20
#define KERNEL 			-15
#define INTERRUPT		-10
#define TASK_MM     	0
#define TASK_SERVMAN 	1
#define TASK_DEVMAN		2
#define TASK_FS			3
#define TASK_SYS		4
#define TASK_TTY		5
#define	TASK_RD			6
#define TASK_INITFS		7
#define TASK_SYSFS		8
#define TASK_HD			9
#define TASK_EXT2_FS	10
#define TASK_PCI		11
#define INIT			12
#define ANY				(NR_TASKS + NR_PROCS + 10)
#define SELF			(-0x8ace)
#define NO_TASK			(NR_TASKS + NR_PROCS + 20)

#define	MAX_TICKS	0x7FFFABCD

/* system call */
#define NR_SYS_CALL	6

/* ipc */
#define SEND		1
#define RECEIVE		2
#define BOTH		3	/* BOTH = (SEND | RECEIVE) */

/* get/set id requests */
#define GS_GETUID	1
#define GS_SETUID	2
#define GS_GETGID	3
#define GS_SETGID	4
#define GS_GETEUID	5
#define GS_GETEGID	6

#define GS_GETHOSTNAME	1
#define GS_SETHOSTNAME	2

/* privilege control requests */
#define PRIVCTL_SET_TASK 1

/* getinfo requests */
#define GETINFO_SYSINFO	1

/* special message flags */
#define MSG_INTERRUPT	0x1 	/* the process has an interrupt message */
#define MSG_KERNLOG 	0x2  	/* the process has a kernellog message */

/**
 * @enum msgtype
 * @brief MESSAGE types
 */
enum msgtype {
	/* 
	 * when hard interrupt occurs, a msg (with type==HARD_INT) will
	 * be sent to some tasks
	 */
	HARD_INT = 1, 						/* 1 */
	/* 
	 * when exception occurs, a msg (with type==FAULT) will
	 * be sent to some tasks
	 */
	FAULT,								/* 2 */
	/* when kernel wants to output kernel messages, a msg (with type==KERNLOG) will
	 * be sent to some tasks(tty, log, etc.)
	 */
	KERN_LOG,							/* 3 */

	/* SYS task */
	GET_TICKS, GET_PID, GET_RTC_TIME, FTIME, BREAK, PTRACE, GTTY, STTY, UNAME,
	PROF, PHYS, LOCK, MPX, GET_TIME_OF_DAY,	GETSETHOSTNAME,			/* 3 ~ 17 */

	/* FS */
	OPEN, CLOSE, READ, WRITE, LSEEK, STAT, FSTAT, UNLINK, MOUNT, UMOUNT, MKDIR, 
	CHROOT, CHDIR, FCHDIR, ACCESS, UMASK, DUP, IOCTL, FCNTL,	/* 18 ~ 36 */
	/* FS & TTY */
	SUSPEND_PROC, RESUME_PROC,				/* 37 ~ 38 */

	/* MM */
	EXEC, WAIT, KILL, ACCT, SBRK, GETSETID, ALARM, SIGACTION, PROCCTL, MMAP,	/* 39 ~ 48 */

	/* FS & MM */
	FORK, EXIT,					/* 49 ~ 50 */

	/* SERVMAN */
	SERVICE_UP, SERVICE_DOWN,		/* 51 ~ 52 */

	/* DEVMAN */
	ANNOUNCE_DEVICE, GET_DRIVER,	/* 53 ~ 54 */

	/* TTY, SYS, FS, MM, etc */		
	SYSCALL_RET,						/* 55 */

	/* message type for fs request */
	FSREQ_RET = 1001,
    FS_REGISTER,
	FS_PUTINODE,					/* 1001 ~ 1011 */
	FS_LOOKUP,
    FS_MOUNTPOINT,
    FS_READSUPER,
    FS_STAT,
    FS_RDWT,
    FS_CREATE,
    FS_FTRUNC,
    FS_SYNC,
    FS_MMAP,

	/* message type for drivers */
	DEV_OPEN = 2001,
	DEV_CLOSE,
	DEV_READ,
	DEV_WRITE,
	DEV_IOCTL,						/* 2001 ~ 2005 */

	/* message for sysfs */
	SYSFS_PUBLISH = 2100,				/* 2100 */
};

/* macros for messages */
#define	FD			u.m3.m3i1
#define NEWFD		u.m3.m3i2
#define	PATHNAME	u.m3.m3p1
#define	FLAGS		u.m3.m3i1
#define	NAME_LEN	u.m3.m3i2
#define	BUF_LEN		u.m3.m3i3
#define MODE		u.m3.m3i4
#define	CNT			u.m3.m3i2
#define NEWID		u.m3.m3i1
#define	REQUEST		u.m3.m3i2
#define	PROC_NR		u.m3.m3i3
#define	DEVICE		u.m3.m3i4
#define	POSITION	u.m3.m3l1
#define	BUF			u.m3.m3p2
#define	OFFSET		u.m3.m3i2
#define	WHENCE		u.m3.m3i3
#define SECONDS		u.m3.m3i1
#define SIGNR		u.m3.m3i1
#define NEWSA		u.m3.m3p1
#define OLDSA		u.m3.m3p2
#define TARGET		u.m3.m3i4

#define	PID			u.m3.m3i2
#define	RETVAL		u.m3.m3i1
#define	STATUS		u.m3.m3i1

/* macros for data_copy */
#define SRC_PID		u.m3.m3i1
#define SRC_ADDR	u.m3.m3p1
#define SRC_SEG		u.m3.m3l1
#define DEST_PID	u.m3.m3i2
#define DEST_ADDR	u.m3.m3p2
#define DEST_SEG	u.m3.m3l2

/* macros for mount */
#define MSOURCE		u.m4.m4p1
#define MTARGET		u.m4.m4p2
#define MLABEL		u.m4.m4p3
#define MDATA		u.m4.m4p4
#define MFLAGS		u.m4.m4l1
#define MNAMELEN1	u.m4.m4i1
#define MNAMELEN2	u.m4.m4i2
#define MNAMELEN3	u.m4.m4i3

/* macros for fault */
#define FAULT_NR	u.m3.m3i1
#define FAULT_ADDR	u.m3.m3i2
#define FAULT_PROC	u.m3.m3i3
#define FAULT_ERRCODE u.m3.m3i4
#define FAULT_STATE	u.m3.m3l1

/* ioctl requests */
#define	DIOCTL_GET_GEO	1

/* Hard Drive */
#define SECTOR_SIZE		512
#define SECTOR_BITS		(SECTOR_SIZE * 8)
#define SECTOR_SIZE_SHIFT	9

/* major device numbers (corresponding to kernel/global.c::dd_map[]) */
#define	NO_DEV			0
#define	DEV_RD			1
#define	DEV_FLOPPY		2
#define	DEV_HD			3
#define	DEV_CHAR_TTY		4
/* make device number from major and minor numbers */
#define	MAJOR_SHIFT		8
#define MINOR_MASK		((1U << MAJOR_SHIFT) - 1)
#define	MAKE_DEV(a,b)		(((a) << MAJOR_SHIFT) | (b))
/* separate major and minor numbers from device number */
#define	MAJOR(x)		((unsigned int)(x >> MAJOR_SHIFT))
#define	MINOR(x)		((unsigned int)(x & MINOR_MASK))

#define	INVALID_INODE		0
#define	ROOT_INODE		1

#define MINOR_INITRD    120

#define	MAX_DRIVES		2
#define	NR_PART_PER_DRIVE	4
#define	NR_SUB_PER_PART		16
#define	NR_SUB_PER_DRIVE	(NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define	NR_PRIM_PER_DRIVE	(NR_PART_PER_DRIVE + 1)

/**
 * @def MAX_PRIM
 * Defines the max minor number of the primary partitions.
 * If there are 2 disks, prim_dev ranges in hd[0-9], this macro will
 * equals 9.
 */

#define	MAX_PRIM		(MAX_DRIVES * NR_PRIM_PER_DRIVE - 1)

#define	MAX_SUBPARTITIONS	(NR_SUB_PER_DRIVE * MAX_DRIVES)

/* device numbers of hard disk */
#define MINOR_hd1 		0x1
#define	MINOR_hd1a		0x10
#define	MINOR_hd2a		(MINOR_hd1a+NR_SUB_PER_PART)

#define	P_PRIMARY	0
#define	P_EXTENDED	1

#define NO_PART		0x00	/* unused entry */
#define EXT_PART	0x05	/* extended partition */

#define	NR_FILES	64
#define	NR_FILE_DESC	64	/* FIXME */
#define	NR_INODE	64	/* FIXME */
#define NR_VFS_MOUNT	16

/* INODE::i_mode (octal, lower 12 bits reserved) */
/* Flag bits for i_mode in the inode. */
#define I_TYPE_MASK		I_TYPE
#define I_TYPE          0170000	/* this field gives inode type */
#define I_UNIX_SOCKET	0140000 /* unix domain socket */
#define I_SYMBOLIC_LINK 0120000	/* file is a symbolic link */
#define I_REGULAR       0100000	/* regular file, not dir or special */
#define I_BLOCK_SPECIAL 0060000	/* block special file */
#define I_DIRECTORY     0040000	/* file is a directory */
#define I_CHAR_SPECIAL  0020000	/* character special file */
#define I_NAMED_PIPE    0010000	/* named pipe (FIFO) */
#define I_SET_UID_BIT   0004000	/* set effective uid_t on exec */
#define I_SET_GID_BIT   0002000	/* set effective gid_t on exec */
#define I_SET_STCKY_BIT 0001000	/* sticky bit */ 
#define ALL_MODES       0007777	/* all bits for user, group and others */
#define RWX_MODES       0000777	/* mode bits for RWX only */
#define R_BIT           0000004	/* Rwx protection bit */
#define W_BIT           0000002	/* rWx protection bit */
#define X_BIT           0000001	/* rwX protection bit */
#define I_NOT_ALLOC     0000000	/* this inode is free */

#define	is_special(m)	((((m) & I_TYPE_MASK) == I_BLOCK_SPECIAL) ||	\
			 (((m) & I_TYPE_MASK) == I_CHAR_SPECIAL))

#define	NR_DEFAULT_FILE_SECTS	2048 /* 2048 * 512 = 1MB */

#define suser() (current->euid == 0)

/* super user uid */
#define SU_UID	((uid_t)0)

#endif /* _CONST_H_ */
