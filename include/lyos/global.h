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

#ifdef __i386__
#include <asm/protect.h>
#include <asm/page.h>
#endif

#ifdef __arm__
#include <asm/page.h>
#endif

#include <asm/type.h>

#include <lyos/param.h>
#include <lyos/priv.h>

EXTERN	int	jiffies;
extern  struct clocksource * curr_clocksource;

#ifdef __i386__
EXTERN	u8                  gdt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	struct descriptor	gdt[GDT_SIZE];
EXTERN	u8                  idt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	struct gate         idt[IDT_SIZE];
EXTERN  pde_t *             initial_pgd;
EXTERN  void*               lapic_addr;
EXTERN  void*               lapic_vaddr;
EXTERN  void*               lapic_eoi_addr;
EXTERN  u32                 bsp_cpu_id, bsp_lapic_id;
EXTERN  struct tss          tss[CONFIG_SMP_MAX_CPUS];
#endif

#ifdef __arm__
EXTERN  struct machine_desc* machine_desc;
extern  pde_t               initial_pgd[];
EXTERN  struct tss          tss[CONFIG_SMP_MAX_CPUS];
#endif

#ifdef __riscv
EXTERN  u32                 bsp_cpu_id;
extern  pde_t               initial_pgd[];
EXTERN  unsigned long       va_pa_offset;
EXTERN  struct tss          tss[CONFIG_SMP_MAX_CPUS];
#endif

extern  int booting_cpu;
extern  int booted_aps;
EXTERN  int ncpus;
EXTERN  u64 cpu_hz[CONFIG_SMP_MAX_CPUS];

extern	char		task_stack[];
extern	struct proc	proc_table[];
extern  struct priv priv_table[];
extern  struct boot_proc boot_procs[];

EXTERN  struct proc *   mmrequest;  /* first proc in mm request queue */

EXTERN  int system_hz;

extern  irq_hook_t  irq_hooks[];

extern  int         err_code;

extern struct sysinfo sysinfo;
EXTERN struct sysinfo * sysinfo_user;

/* Multiboot */
extern  kinfo_t     kinfo;
EXTERN  int         mb_mod_count;
EXTERN  void*       mb_mod_addr;
EXTERN  int         mb_magic;
EXTERN  int         mb_flags;
EXTERN  void*       initial_boot_params;

/* KERNEL LOG */
extern struct kern_log kern_log;

extern struct machine machine;

#if CONFIG_PROFILING
#include <lyos/profile.h>

EXTERN char profile_sample_buf[KPROF_SAMPLE_BUFSIZE];
extern int kprofiling;
EXTERN struct kprof_info kprof_info;

#endif
