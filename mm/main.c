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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include <lyos/vm.h>
#include "multiboot.h"
#include "page.h"
#include <elf.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include "global.h"

PRIVATE int free_mem_size;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern pde_t pgd0;

PUBLIC void __lyos_init();

PRIVATE void init_mm();
PRIVATE struct mmproc * init_mmproc(endpoint_t endpoint);
PRIVATE void spawn_bootproc(struct mmproc * mmp, struct boot_proc * bp);
PRIVATE void print_memmap();

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
			mm_msg.RETVAL = ENOSYS;
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
	int i;
	
	get_kinfo(&kernel_info);
	print_memmap();

	/* initialize hole table */
	mem_init(mem_start, free_mem_size);
	vir_bytes vmalloc_start = (kernel_info.kernel_end_pde + MAX_PAGEDIR_PDES) * ARCH_BIG_PAGE_SIZE;
	vmem_init(vmalloc_start, 
		VMALLOC_END - vmalloc_start);
	pt_init();

	__lyos_init();
	init_mmproc(TASK_MM);

	/* setup memory region for tasks so they can malloc */
	int region_size = NR_TASKS * sizeof(struct vir_region) * 2;
	struct vir_region * rp = (struct vir_region *)alloc_vmem(NULL, region_size * 2);
	struct proc * p = proc_table;
	struct boot_proc * bp;
	for (bp = kernel_info.boot_procs; bp < &kernel_info.boot_procs[NR_BOOT_PROCS]; bp++, i++, rp++, p++) {
		struct mmproc * mmp = init_mmproc(bp->endpoint);
		mmp->slot = i;
		INIT_LIST_HEAD(&(mmp->mem_regions));
		mmp->flags = MMPF_INUSE;

		if (bp->proc_nr == TASK_MM) continue;

		spawn_bootproc(mmp, bp);

		if (!(PST_IS_SET(p, PST_BOOTINHIBIT) || i == TASK_MM) && (i != INIT)) {
			phys_region_init(&(rp->phys_block), 1);
			/* prepare heap */
			rp->vir_addr = (void*)0x1000;
			p->brk = 0x1000;
			rp->length = 0;
			rp->flags = RF_WRITABLE;
			list_add(&(rp->list), &(mmp->mem_regions));
		}
		vmctl(VMCTL_MMINHIBIT_CLEAR, i);
    }
}

PRIVATE struct mmproc * init_mmproc(endpoint_t endpoint)
{
	struct mmproc * mmp;
	struct boot_proc * bp;
	for (bp = kernel_info.boot_procs; bp < &kernel_info.boot_procs[NR_BOOT_PROCS]; bp++) {
		if (bp->endpoint == endpoint) {
			mmp = &mmproc_table[bp->proc_nr];
			mmp->flags = MMPF_INUSE;
			mmp->endpoint = endpoint;

			return mmp;
		}
	}
	panic("MM: no mmproc");
	return NULL;
}

PRIVATE void spawn_bootproc(struct mmproc * mmp, struct boot_proc * bp)
{
	if (pgd_new(&(mmp->pgd))) panic("MM: spawn_bootproc: pgd_new failed");
	if (pgd_bind(mmp, &mmp->pgd)) panic("MM: spawn_bootproc: pgd_bind failed");
}

PRIVATE void print_memmap()
{
	int usable_memsize = 0;
	int reserved_memsize = 0;
	int i;

	printl("Kernel-provided physical RAM map:\n");
	struct kinfo_mmap_entry * mmap;
	for (i = 0, mmap = kernel_info.memmaps; i < kernel_info.memmaps_count; i++, mmap++) {
		u64 last_byte = mmap->addr + mmap->len;
		u32 base_h = (u32)((mmap->addr & 0xFFFFFFFF00000000L) >> 32),
			base_l = (u32)(mmap->addr & 0xFFFFFFFF);
		u32 last_h = (u32)((last_byte & 0xFFFFFFFF00000000L) >> 32),
			last_l = (u32)(last_byte & 0xFFFFFFFF);
		printl("  [mem %08x%08x-%08x%08x] %s\n", base_h, base_l, last_h, last_l, 
			(mmap->type == KINFO_MEMORY_AVAILABLE) ? "usable" : "reserved");

		if (mmap->type == KINFO_MEMORY_AVAILABLE) 
			usable_memsize += mmap->len;
		else 
			reserved_memsize += mmap->len;
	}

	unsigned int text_start = (unsigned int)*(&_text), text_end = (unsigned int)*(&_etext), text_len = text_end - text_start;
	unsigned int data_start = (unsigned int)*(&_data), data_end = (unsigned int)*(&_edata), data_len = data_end - data_start;
	unsigned int bss_start = (unsigned int)*(&_bss), bss_end = (unsigned int)*(&_ebss), bss_len = bss_end - bss_start;

	usable_memsize = usable_memsize - text_len - data_len - bss_len;
	printl("Memory: %dk/%dk available (%dk kernel code, %dk data, %dk reserved)\n", 
						usable_memsize / 1024, memory_size / 1024,
						text_len / 1024, (data_len + bss_len) / 1024,
						reserved_memsize / 1024);
	
	vir_bytes vmalloc_start = (kernel_info.kernel_end_pde + MAX_PAGEDIR_PDES) * ARCH_BIG_PAGE_SIZE;

	printl("Virtual kernel memory layout:\n");
	printl("  .text   : 0x%08x - 0x%08x  (%dkB)\n", text_start, text_end, text_len / 1024);
	printl("  .data   : 0x%08x - 0x%08x  (%dkB)\n", data_start, data_end, data_len / 1024);
	printl("  .bss    : 0x%08x - 0x%08x  (%dkB)\n", bss_start, bss_end, bss_len / 1024);
	printl("  vmalloc : 0x%08x - 0x%08x  (%dkB)\n", vmalloc_start, VMALLOC_END, (VMALLOC_END - vmalloc_start) / 1024);
	printl("  fixmap  : 0x%08x - 0x%08x  (%dkB)\n", FIXMAP_START, FIXMAP_END, (FIXMAP_END - FIXMAP_START) / 1024);

	mem_start = PROCS_BASE;
	free_mem_size = memory_size - mem_start;
}
