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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "multiboot.h"
#include "page.h"
#include "acpi.h"
#include "arch_const.h"
#include <lyos/log.h>
#include <lyos/spinlock.h>

extern char _end[];
extern pde_t pgd0;
PRIVATE pde_t * user_pgd = NULL;
PRIVATE int first_pgd = 0;
PUBLIC void * k_stacks;

/*======================================================================*
                            cstart
 *======================================================================*/
PUBLIC void cstart(struct multiboot_info *mboot, u32 mboot_magic)
{
	memory_size = 0;
	memset(&kinfo, 0, sizeof(kinfo_t));

	kinfo.magic = KINFO_MAGIC;
	mb_magic = mboot_magic;

	int mb_mmap_addr, mb_mmap_len;

	/* grub provides physical address, we want virtual address */
	mboot = (struct multiboot_info *)((int)mboot + KERNEL_VMA);
	mb_mmap_addr = mboot->mmap_addr + KERNEL_VMA;
	mb_mmap_len = mboot->mmap_length;

	kinfo.mods_count = mb_mod_count = mboot->mods_count;

	kinfo.memmaps_count = -1;
	struct multiboot_mmap_entry * mmap = (struct multiboot_mmap_entry *)mb_mmap_addr;
	while ((unsigned int)mmap < mb_mmap_len + mb_mmap_addr) {
		kinfo.memmaps_count++;
		kinfo.memmaps[kinfo.memmaps_count].addr = mmap->addr;
		kinfo.memmaps[kinfo.memmaps_count].len = mmap->len;
		kinfo.memmaps[kinfo.memmaps_count].type = mmap->type;
		memory_size += mmap->len;
		mmap = (struct multiboot_mmap_entry *)((unsigned int)mmap + mmap->size + sizeof(unsigned int));
	}

	mb_mod_addr = mboot->mods_addr + KERNEL_VMA;
	multiboot_module_t * last_mod = (multiboot_module_t *)mboot->mods_addr;
	last_mod += mb_mod_count - 1;
	int pgd_start = (unsigned)last_mod->mod_end;

	/* setup kernel page table */
	initial_pgd = (pde_t *)((int)&pgd0 - KERNEL_VMA);
	first_pgd = (pgd_start + 0x1000) & ARCH_VM_ADDR_MASK;
	pte_t * pt = (pte_t*)(first_pgd + (NR_TASKS + NR_NATIVE_PROCS) * 0x1000);	/* 4k align */
	PROCS_BASE = (int)(pt + 1024 * 1024) & ARCH_VM_ADDR_MASK;
    kernel_pts = PROCS_BASE / PT_MEMSIZE;
    if (PROCS_BASE % PT_MEMSIZE != 0) {
    	kernel_pts++;
    	PROCS_BASE = kernel_pts * PT_MEMSIZE;
    }
    kinfo.kernel_start_pde = ARCH_PDE(KERNEL_VMA);
	kinfo.kernel_end_pde = ARCH_PDE(KERNEL_VMA) + kernel_pts;
	setup_paging(initial_pgd, pt, kernel_pts);
	
	/* initial_pgd --> physical initial pgd */

	/* setup user page table */
	int i, j;
	for (i = 0; i < NR_TASKS + NR_NATIVE_PROCS; i++) {
		user_pgd = (pde_t*)(first_pgd + i * PGD_SIZE + KERNEL_VMA);
		for (j = 0; j < kernel_pts; j++) { 
        	user_pgd[j + ARCH_PDE(KERNEL_VMA)] = initial_pgd[j];
    	}
    }

	init_prot();

	mb_flags = mboot->flags;

	k_stacks = &k_stacks_start;

	sysinfo_user = NULL;
	memset(&kern_log, 0, sizeof(struct kern_log));
	spinlock_init(&kern_log.lock);
}

PUBLIC void init_arch()
{
	int i, j, eflags, prio, quantum;
    u32 codeseg, dataseg;

	struct task * t;
	struct proc * p = proc_table;

	char * stk = task_stack + STACK_SIZE_TOTAL;

	acpi_init();

	for (i = 0; i < NR_TASKS + NR_PROCS; i++,p++,t++) {
		spinlock_init(&p->lock);
		
		if (i >= NR_TASKS + NR_NATIVE_PROCS) {
			p->state = PST_FREE_SLOT;
			continue;
		}

	    if (i < NR_TASKS) {     /* TASK */
            t	= task_table + i;
            codeseg = SELECTOR_TASK_CS | RPL_TASK;
            dataseg = SELECTOR_TASK_DS | RPL_TASK;
            eflags  = 0x1202;/* IF=1, IOPL=1, bit 2 is always 1 */
			prio    = TASK_PRIO;
			quantum = TASK_QUANTUM;
        } else {                  /* USER PROC */
            t	= user_proc_table + (i - NR_TASKS);
            codeseg = SELECTOR_USER_CS | RPL_USER;
            dataseg = SELECTOR_USER_DS | RPL_USER;
            eflags  = 0x202;	/* IF=1, bit 2 is always 1 */
			prio    = USER_PRIO;
			quantum = USER_QUANTUM;
        }

		strcpy(p->name, t->name);	/* name of the process */
		p->p_parent = NO_TASK;

		if (strcmp(t->name, "INIT") != 0) {

			if (i == TASK_MM) {
				/* use kernel page table */ 
				p->pgd.phys_addr = initial_pgd;
				p->pgd.vir_addr = (pde_t *)((int)initial_pgd + KERNEL_VMA);

				p->seg.cr3_phys = (u32)initial_pgd;
				p->seg.cr3_vir = (u32 *)((int)initial_pgd + KERNEL_VMA);
			} else {
				p->seg.cr3_phys =0;
				p->pgd.phys_addr = (pte_t *)(first_pgd + i * PGD_SIZE);
				p->pgd.vir_addr = (pte_t *)(first_pgd + i * PGD_SIZE + KERNEL_VMA);
			}

			for (j = 0; j < I386_VM_DIR_ENTRIES; j++) {
				if (p->pgd.vir_addr[j] != 0) p->pgd.vir_pts[j] = (pte_t *)(((int)(p->pgd.vir_addr[j]) + KERNEL_VMA) & 0xfffff000);
			}	

		} else {		/* INIT process */
			p->seg.cr3_phys =0;
			p->pgd.phys_addr = (pte_t *)(first_pgd + i * PGD_SIZE);
			p->pgd.vir_addr = (pte_t *)(first_pgd + i * PGD_SIZE + KERNEL_VMA);

			memset(&(p->pgd.vir_pts), 0, sizeof(p->pgd.vir_pts));
			for (j = ARCH_PDE(KERNEL_VMA); j < ARCH_PDE(KERNEL_VMA) + kernel_pts; j++) {
				p->pgd.vir_pts[j] = (pte_t *)(((int)(p->pgd.vir_addr[j]) + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
			}
		}

		p->regs.cs = codeseg;
		p->regs.ds =
		p->regs.es =
		p->regs.fs =
		p->regs.gs =
		p->regs.ss = dataseg;

		p->regs.eip	= (u32)t->initial_eip;
		p->regs.esp	= (u32)stk;
		p->regs.eflags	= eflags;
		p->seg.trap_style = KTS_INT;

		p->counter = p->quantum_ms = quantum;
		p->priority = prio;

		p->state = 0;

		if (t->initial_eip == NULL) 
			PST_SET(p, PST_BOOTINHIBIT);	/* process is to be loaded by SERVMAN */
		if (i != TASK_MM) 
			PST_SET(p, PST_MMINHIBIT);		/* process is not ready until MM allows it to run */

		p->send_msg = 0;
		p->recv_msg = 0;
		p->recvfrom = NO_TASK;
		p->sendto = NO_TASK;
		p->special_msg = 0;
		p->q_sending = 0;
		p->next_sending = 0;

        p->uid = 0;
        p->gid = 0;

		p->pwd = NULL;
		p->root = NULL;
		p->umask = ~0;

		//p->brk = 0;

		for (j = 0; j < NR_FILES; j++)
			p->filp[j] = 0;

		stk -= t->stacksize;
	}
}
