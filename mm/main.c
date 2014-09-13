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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "multiboot.h"
#include "page.h"
#include <elf.h>
#include "region.h"
#include "proto.h"

PRIVATE int free_mem_size;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern pde_t pgd0;

PRIVATE void init_mm();

/*****************************************************************************
 *                                task_mm
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK MM.
 * 
 *****************************************************************************/
PUBLIC void task_mm()
{
	init_mm();

	while (1) {
		send_recv(RECEIVE, ANY, &mm_msg);
		int src = mm_msg.source;
		int reply = 1;

		int msgtype = mm_msg.type;

		switch (msgtype) {
		case FORK:
			mm_msg.RETVAL = do_fork();
			break;
		case EXIT:
			do_exit(mm_msg.STATUS);
			reply = 0;
			break;
		case WAIT:
			do_wait();
			reply = 0;
			break;
		case KILL:
			mm_msg.RETVAL = do_kill();
			if (mm_msg.RETVAL == SUSPEND) reply = 0;
			break; 
		case SBRK:
			mm_msg.RETVAL = do_sbrk();
			break;
		case GETSETID:
			mm_msg.RETVAL = do_getsetid();
			break;
		case SIGACTION:
			mm_msg.RETVAL = do_sigaction();
			break;
		case ALARM:
			mm_msg.RETVAL = do_alarm();
			break;
		case MMAP:
			mm_msg.RETVAL = do_mmap();
			break;
		case PROCCTL:
			mm_msg.RETVAL = do_procctl();
			break;
		case FAULT:
			do_handle_fault();
			reply = 0;
			break;
		default:
			dump_msg("MM::unknown msg", &mm_msg);
			assert(0);
			break;
		}

		if (reply) {
			mm_msg.type = SYSCALL_RET;
			send_recv(SEND, src, &mm_msg);
		}
	}
}

/*****************************************************************************
 *                                init_mm
 *****************************************************************************/
/**
 * Do some initialization work.
 *
 * Memory info is collected from boot_param, then set the buffer and ramdisk
 * area.
 *
 * 
 *****************************************************************************/
PRIVATE void init_mm()
{	
	int usable_memsize = 0;
	int reserved_memsize = 0;
	if (!(mb_flags & (1 << 6))) panic("Memory map not present!");
	printl("BIOS-provided physical RAM map:\n");
	printl("Memory map located at: 0x%x\n", mb_mmap_addr);
	struct multiboot_mmap_entry * mmap = (struct multiboot_mmap_entry *)(mb_mmap_addr);
	while ((unsigned int)mmap < mb_mmap_len + mb_mmap_addr) {
		u64 last_byte = mmap->addr + mmap->len;
		u32 base_h = (u32)((mmap->addr & 0xFFFFFFFF00000000L) >> 32),
			base_l = (u32)(mmap->addr & 0xFFFFFFFF);
		u32 last_h = (u32)((last_byte & 0xFFFFFFFF00000000L) >> 32),
			last_l = (u32)(last_byte & 0xFFFFFFFF);
		printl("  [mem %08x%08x-%08x%08x] %s\n", base_h, base_l, last_h, last_l, 
			(mmap->type == MULTIBOOT_MEMORY_AVAILABLE) ? "usable" : "reserved");

		if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) 
			usable_memsize += mmap->len;
		else 
			reserved_memsize += mmap->len;

		mmap = (struct multiboot_mmap_entry *)((unsigned int)mmap + mmap->size + sizeof(unsigned int));
	}

	unsigned int text_start = (unsigned int)*(&_text), text_end = (unsigned int)*(&_etext), text_len = text_end - text_start;
	unsigned int data_start = (unsigned int)*(&_data), data_end = (unsigned int)*(&_edata), data_len = data_end - data_start;
	unsigned int bss_start = (unsigned int)*(&_bss), bss_end = (unsigned int)*(&_ebss), bss_len = bss_end - bss_start;

	usable_memsize = usable_memsize - text_len - data_len - bss_len;
	printl("Memory: %dk/%dk available (%dk kernel code, %dk data, %dk reserved)\n", 
						usable_memsize / 1024, memory_size / 1024,
						text_len / 1024, (data_len + bss_len) / 1024,
						reserved_memsize / 1024);
	printl("Physical process memory base: 0x%x\n", PROCS_BASE);
	
	printl("Virtual kernel memory layout:\n");
	printl("  .text   : 0x%08x - 0x%08x  (%dkB)\n", text_start, text_end, text_len / 1024);
	printl("  .data   : 0x%08x - 0x%08x  (%dkB)\n", data_start, data_end, data_len / 1024);
	printl("  .bss    : 0x%08x - 0x%08x  (%dkB)\n", bss_start, bss_end, bss_len / 1024);
	printl("  vmalloc : 0x%08x - 0x%08x  (%dkB)\n", VMALLOC_START, VMALLOC_END, (VMALLOC_END - VMALLOC_START) / 1024);
	printl("  fixmap  : 0x%08x - 0x%08x  (%dkB)\n", FIXMAP_START, FIXMAP_END, (FIXMAP_END - FIXMAP_START) / 1024);
	
	printl("MM: Kernel page directory at physical address: 0x%x\n", initial_pgd);

	mem_start = PROCS_BASE;
	free_mem_size = memory_size - mem_start;

	/* initialize hole table */
	mem_init(mem_start, free_mem_size);
	vmem_init(VMALLOC_START, VMALLOC_END - VMALLOC_START);

	/* setup memory region for tasks so they can malloc */
	int region_size = NR_TASKS * sizeof(struct vir_region) * 2;
	struct vir_region * rp = (struct vir_region *)alloc_vmem(region_size * 2);
	struct proc * p = proc_table;
	int i;
	for (i = 0; i < NR_TASKS; i++, rp++, p++) {
		if (p->state == BLOCKED) continue;
		phys_region_init(&(rp->phys_block), 1);
		/* prepare heap */
		rp->vir_addr = (void*)0x1000;
		p->brk = 0x1000;
		rp->length = 0;
		list_add(&(rp->list), &(p->mem_regions));
    }
}
