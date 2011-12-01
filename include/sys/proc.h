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

#include "signal.h"

struct stackframe {	
	u32	gs;		
	u32	fs;	
	u32	es;	
	u32	ds;	
	u32	edi;	
	u32	esi;	
	u32	ebp;	
	u32	kernel_esp;	
	u32	ebx;	
	u32	edx;	
	u32	ecx;	
	u32	eax;	
	u32	retaddr;
	u32	eip;	
	u32	cs;	
	u32	eflags;	
	u32	esp;	
	u32	ss;	
};


struct proc {
	struct stackframe regs;    /* process registers saved in stack frame */

	u16 ldt_sel;               /* gdt selector giving ldt base and limit */
	struct descriptor ldts[LDT_SIZE]; /* local descs for code and data */

    	int counter;                 /* remained ticks */
    	int priority;

	/* u32 pid;                   /\* process id passed in from MM *\/ */
	char name[16];		   /* name of the process */

	int state;              /**
				    * process flags.
				    * A proc is runnable if state==0
				    */

    	int signal;
	struct sigaction sigaction[32];
	unsigned long alarm;
	long blocked;
    
	MESSAGE * msg;
	int recvfrom;
	int sendto;

	int has_int_msg;           /**
				    * nonzero if an INTERRUPT occurred when
				    * the task is not ready to deal with it.
				    */

	struct proc * q_sending;   /**
				    * queue of procs sending messages to
				    * this proc
				    */
	struct proc * next_sending;/**
				    * next proc in the sending
				    * queue (q_sending)
				    */

	int p_parent; /**< pid of parent process */

	/* user id,group id etc. */
	u16 uid,euid,suid;
	u16 gid,egid,sgid;
	
	//u16 used_math;

	struct inode * pwd;
	struct inode * root;

	int exit_status; /**< for parent */

	struct file_desc * filp[NR_FILES];
};

struct task {
	task_f	initial_eip;
	int	stacksize;
	char	name[32];
};

#define proc2pid(x) (x - proc_table)

/* Number of tasks & processes */
#define NR_TASKS		10
#define NR_PROCS		32
#define NR_NATIVE_PROCS		4
#define FIRST_PROC		proc_table[0]
#define LAST_PROC		proc_table[NR_TASKS + NR_PROCS - 1]

/**
 * All forked proc will use memory above PROCS_BASE.
 *
 * @attention make sure PROCS_BASE is higher than any buffers, such as
 *            fsbuf, mmbuf, etc
 * @see global.c
 * @see global.h
 */
#define	BUFFER_BASE		0xA00000 /* 10 MB */
#define BUFFER_LENGTH	(1 * 1024 * 1024)	/* 1 MB */
#define RAMDISK_BASE	0xC00000 /* 12 MB */
#define RAMDISK_LENGTH	(2880 * 1024 ) /* 2.88 MB */
#define	PROCS_BASE		0xF00000 /* 15 MB */
#define	PROC_IMAGE_SIZE_DEFAULT	0x100000 /*  1 MB */
#define	PROC_ORIGIN_STACK	0x400    /*  1 KB */

#define PAGE_DIR_BASE	0x100000
#define PAGE_TBL_BASE	0x101000
#define PAGE_SIZE	0x1000

/** User paging map **/
#define USER_KERNEL_STACK 0x0
#define USER_KERNEL_STACK_TOP 0x4000
#define USER_STACK 0x4000
#define USER_STACK_TOP 0x8000
#define USER_KERNEL_START 0x100000
#define USER_KERNEL_END 0x200000
#define USER_ALLOCATED_START 0x200000
#define USER_ALLOCATED_END 0x300000
#define USER_CODE_START 0x300000
#define USER_CODE_END 0x400000
#define USER_END 0x400000
/** The allocated user pages' states **/
#define ALLOC_STATE_UNUSED 0
#define ALLOC_STATE_USED 1
/** The kernel paged used sign, an AVL bit of a pte entry **/
#define KERNEL_USER_USED_MASK 512

/* stacks of tasks */
#define	STACK_SIZE_DEFAULT	0x4000 /* 16 KB */
#define STACK_SIZE_TTY		STACK_SIZE_DEFAULT
#define STACK_SIZE_SYS		STACK_SIZE_DEFAULT
#define STACK_SIZE_HD		STACK_SIZE_DEFAULT
#define STACK_SIZE_FS		STACK_SIZE_DEFAULT
#define STACK_SIZE_MM		STACK_SIZE_DEFAULT
#define STACK_SIZE_RD		STACK_SIZE_DEFAULT
#define STACK_SIZE_FD		STACK_SIZE_DEFAULT
#define STACK_SIZE_SCSI		STACK_SIZE_DEFAULT
#define STACK_SIZE_PCI		STACK_SIZE_DEFAULT
#define STACK_SIZE_INET		STACK_SIZE_DEFAULT
#define STACK_SIZE_LYOS_FS	STACK_SIZE_DEFAULT
#define STACK_SIZE_INIT		STACK_SIZE_DEFAULT
#define STACK_SIZE_TESTA	STACK_SIZE_DEFAULT
#define STACK_SIZE_TESTB	STACK_SIZE_DEFAULT
#define STACK_SIZE_TESTC	STACK_SIZE_DEFAULT

#define STACK_SIZE_TOTAL	(STACK_SIZE_TTY + \
				STACK_SIZE_SYS + \
				STACK_SIZE_HD + \
				STACK_SIZE_FS + \
				STACK_SIZE_MM + \
				STACK_SIZE_RD + \
				STACK_SIZE_FD + \
				STACK_SIZE_SCSI + \
				STACK_SIZE_PCI + \
				STACK_SIZE_INET + \
				STACK_SIZE_LYOS_FS + 	\
				STACK_SIZE_INIT + \
				STACK_SIZE_TESTA + \
				STACK_SIZE_TESTB + \
				STACK_SIZE_TESTC)

