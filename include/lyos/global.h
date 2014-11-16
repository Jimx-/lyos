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

/* EXTERN is defined as extern except in global.c */
#ifdef	GLOBAL_VARIABLES_HERE
#undef	EXTERN
#define	EXTERN
#endif

#if(ARCH == x86)
#include "protect.h"
#include "page.h"
#endif

#include <lyos/param.h>

EXTERN	int	jiffies;

#if (ARCH == x86)
EXTERN	u8			        gdt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	struct descriptor	gdt[GDT_SIZE];
EXTERN	u8			        idt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	struct gate		    idt[IDT_SIZE];
EXTERN  pde_t *             initial_pgd;
EXTERN  unsigned int        lapic_addr;
#endif

extern  int booting_cpu;
EXTERN  int ncpus;

EXTERN	int	key_pressed; /**
			      * used for clock_handler
			      * to wake up TASK_TTY when
			      * a key is pressed
			      */

EXTERN  struct tss  tss[CONFIG_SMP_MAX_CPUS];

extern	char		task_stack[];
extern	struct proc	proc_table[];
extern  struct task	task_table[];
extern  struct task	user_proc_table[];
extern  struct boot_proc boot_procs[];

extern  irq_hook_t  irq_hooks[];

extern  int         err_code;

extern struct sysinfo sysinfo;
EXTERN struct sysinfo * sysinfo_user;

/* Multiboot */
extern  kinfo_t     kinfo;
EXTERN  int         mb_mod_count;
EXTERN  int         mb_mod_addr;
EXTERN  int         mb_magic;
EXTERN  int         mb_flags;

/* MM */
EXTERN	MESSAGE			mm_msg;

EXTERN  unsigned int        PROCS_BASE;
EXTERN  int                 kernel_pts;
EXTERN	unsigned int        memory_size;
EXTERN	int 				mem_start;

/* KERNEL LOG */
extern struct kern_log kern_log;
