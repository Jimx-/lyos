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
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "multiboot.h"
#include "page.h"

int get_kernel_map(unsigned int * b, unsigned int * l);

extern char _end[];
extern pde_t pgd0;
PRIVATE pde_t * user_pgd = NULL;
PRIVATE int first_pgd = 0;

/*======================================================================*
                            cstart
 *======================================================================*/
PUBLIC void cstart(struct multiboot_info *mboot, u32 mboot_magic)
{
	memory_size = 0;

	mb_magic = mboot_magic;

	/* grub provides physical address, we want virtual address */
	mboot = (struct multiboot_info *)((int)mboot + KERNEL_VMA);
	mb_mmap_addr = mboot->mmap_addr + KERNEL_VMA;
	mb_mmap_len = mboot->mmap_length;

	mb_mod_count = mboot->mods_count;

	struct multiboot_mmap_entry * mmap = (struct multiboot_mmap_entry *)mb_mmap_addr;
	while ((unsigned int)mmap < mb_mmap_len + mb_mmap_addr) {
		memory_size += mmap->len;
		mmap = (struct multiboot_mmap_entry *)((unsigned int)mmap + mmap->size + sizeof(unsigned int));
	}

	multiboot_module_t * initrd_mod = (multiboot_module_t *)mboot->mods_addr;
	mb_mod_addr = mboot->mods_addr + KERNEL_VMA;
	int pgd_start = (unsigned)initrd_mod->mod_end;

	/* setup kernel page table */
	initial_pgd = (pde_t *)((int)&pgd0 - KERNEL_VMA);
	first_pgd = (pgd_start + 0x1000) & 0xfffff000;
	pte_t * pt = (pte_t*)(first_pgd + (NR_TASKS + NR_NATIVE_PROCS) * 0x1000);	/* 4k align */
	PROCS_BASE = (int)(pt + 1024 * 1024) & 0xfffff000;
    kernel_pts = PROCS_BASE / PT_MEMSIZE;
    if (PROCS_BASE % PT_MEMSIZE != 0) {
    	kernel_pts++;
    	PROCS_BASE = kernel_pts * PT_MEMSIZE;
    }
	setup_paging(memory_size, initial_pgd, pt, kernel_pts);
	
	/* setup user page table */
	int i, j;
	for (i = 0; i < NR_TASKS + NR_NATIVE_PROCS; i++) {
		user_pgd = (pde_t*)(first_pgd + i * 0x1000);
		for (j = 0; j < kernel_pts; j++) {
        	user_pgd[j + KERNEL_VMA / 0x400000] = initial_pgd[j + KERNEL_VMA / 0x400000];
    	}
    }

	init_desc(&gdt[0], 0, 0, 0);
	init_desc(&gdt[1], 0, 0xfffff, DA_CR  | DA_32 | DA_LIMIT_4K);
	init_desc(&gdt[2], 0, 0xfffff, DA_DRW  | DA_32 | DA_LIMIT_4K);
	init_desc(&gdt[3], 0x0B8000 + KERNEL_VMA, 0xffff, DA_DRW | DA_32 | DA_DPL3);

	u16* p_gdt_limit = (u16*)(&gdt_ptr[0]);
	u32* p_gdt_base = (u32*)(&gdt_ptr[2]);
	*p_gdt_limit = GDT_SIZE * sizeof(struct descriptor) - 1;
	*p_gdt_base = (u32)&gdt;

	u16* p_idt_limit = (u16*)(&idt_ptr[0]);
	u32* p_idt_base  = (u32*)(&idt_ptr[2]);
	*p_idt_limit = IDT_SIZE * sizeof(struct gate) - 1;
	*p_idt_base  = (u32)&idt;

	init_prot();

	mb_flags = mboot->flags;
	kernel_file = (unsigned int)&(mboot->u.elf_sec);

	unsigned char * dev_no = (unsigned char *)((int)&(mboot->boot_device));

	int major;
	if (dev_no[3] == 0x80) major = DEV_HD;
	else major = DEV_FLOPPY;

	int minor = dev_no[2] + 1;
	ROOT_DEV = MAKE_DEV(major, minor);
}

PUBLIC void init_arch()
{
	int i, j, eflags, prio;
        u8  rpl;
        u8  priv; /* privilege */

	struct task * t;
	struct proc * p = proc_table;

	char * stk = task_stack + STACK_SIZE_TOTAL;

	for (i = 0; i < NR_TASKS + NR_PROCS; i++,p++,t++) {
		if (i >= NR_TASKS + NR_NATIVE_PROCS) {
			p->state = FREE_SLOT;
			continue;
		}

	    	if (i < NR_TASKS) {     /* TASK */
                        t	= task_table + i;
                        priv	= PRIVILEGE_TASK;
                        rpl     = RPL_TASK;
                        eflags  = 0x1202;/* IF=1, IOPL=1, bit 2 is always 1 */
			            prio    = 15;
                }
                else {                  /* USER PROC */
                        t	= user_proc_table + (i - NR_TASKS);
                        priv	= PRIVILEGE_USER;
                        rpl     = RPL_USER;
                        eflags  = 0x202;	/* IF=1, bit 2 is always 1 */
			            prio    = 5;
                }

		strcpy(p->name, t->name);	/* name of the process */
		p->p_parent = NO_TASK;

		if (strcmp(t->name, "INIT") != 0) {
			p->ldts[INDEX_LDT_C]  = gdt[SELECTOR_KERNEL_CS >> 3];
			p->ldts[INDEX_LDT_RW] = gdt[SELECTOR_KERNEL_DS >> 3];

			/* change the DPLs */
			p->ldts[INDEX_LDT_C].attr1  = DA_C   | priv << 5;
			p->ldts[INDEX_LDT_RW].attr1 = DA_DRW | priv << 5;

			/* use kernel page table */
			/*if (strcmp(t->name, "VFS") == 0) {
				p->pgd.phys_addr = (pte_t *)(first_pgd + i * PGD_SIZE);
				p->pgd.vir_addr = (pte_t *)(first_pgd + i * PGD_SIZE + KERNEL_VMA);
			} else { */
				p->pgd.phys_addr = initial_pgd;
				p->pgd.vir_addr = initial_pgd + KERNEL_VMA;
			/*}*/

			for (j = 0; j < I386_VM_DIR_ENTRIES; j++) {
				p->pgd.vir_pts[j] = (pte_t *)((p->pgd.phys_addr[j] + KERNEL_VMA) & 0xfffff000);
			}			
		}
		else {		/* INIT process */
			unsigned int k_base;
			unsigned int k_limit;
			get_kernel_map(&k_base, &k_limit);
			init_desc(&p->ldts[INDEX_LDT_C],
				  0, VM_STACK_TOP >> LIMIT_4K_SHIFT,
				  DA_32 | DA_LIMIT_4K | DA_C | priv << 5);

			init_desc(&p->ldts[INDEX_LDT_RW],
				  0, VM_STACK_TOP >> LIMIT_4K_SHIFT,
				  DA_32 | DA_LIMIT_4K | DA_DRW | priv << 5);
			
			//p->pgd.phys_addr = user_pgd;
			//p->pgd.vir_addr = user_pgd;
			p->pgd.phys_addr = (pte_t *)(first_pgd + i * PGD_SIZE);
			//FIXME: should be p->pgd.vir_addr = user_pgd + KERNEL_VMA;
			p->pgd.vir_addr = (pte_t *)(first_pgd + i * PGD_SIZE);

			for (j = KERNEL_VMA / 0x400000; j < KERNEL_VMA / 0x400000 + 4; j++) {
				p->pgd.vir_pts[j] = (pte_t *)((p->pgd.phys_addr[j] + KERNEL_VMA) & 0xfffff000);
			}
		}

		p->regs.cs = INDEX_LDT_C << 3 |	SA_TIL | rpl;
		p->regs.ds =
		p->regs.es =
		p->regs.fs =
		p->regs.ss = INDEX_LDT_RW << 3 | SA_TIL | rpl;
		p->regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;
		p->regs.eip	= (u32)t->initial_eip;
		p->regs.esp	= (u32)stk;
		p->regs.eflags	= eflags;

		p->counter = p->priority = prio;

		p->state = 0;
		p->msg = 0;
		p->recvfrom = NO_TASK;
		p->sendto = NO_TASK;
		p->has_int_msg = 0;
		p->q_sending = 0;
		p->next_sending = 0;

        p->uid = 0;
        p->gid = 0;

		p->pwd = NULL;
		p->root = NULL;
		p->umask = ~0;

		INIT_LIST_HEAD(&(p->mem_regions));
		p->brk = 0;

		for (j = 0; j < NR_FILES; j++)
			p->filp[j] = 0;

		stk -= t->stacksize;
	}

	k_reenter = 0;

	current	= proc_table;
}
