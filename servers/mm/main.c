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
#include <lyos/fs.h>
#include <sys/mman.h>
#include "libexec/libexec.h"
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
	
	while (TRUE) {
		send_recv(RECEIVE, ANY, &mm_msg);
		int src = mm_msg.source;
		int reply = 1;

		int msgtype = mm_msg.type;

		switch (msgtype) {
		case PM_MM_FORK:
			mm_msg.RETVAL = do_fork();
			break;
		case BRK:
			mm_msg.RETVAL = do_brk();
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

	struct boot_proc * bp;
	for (i = -NR_TASKS, bp = kernel_info.boot_procs; bp < &kernel_info.boot_procs[NR_BOOT_PROCS]; bp++, i++) {
		if (bp->proc_nr < 0) continue;

		struct mmproc * mmp = init_mmproc(bp->endpoint);
		mmp->slot = i;
		INIT_LIST_HEAD(&(mmp->mem_regions));
		mmp->flags = MMPF_INUSE;

		if (bp->proc_nr == TASK_MM) continue;

		spawn_bootproc(mmp, bp);

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
	panic("no mmproc");
	return NULL;
}

struct mm_exec_info {
	struct exec_info execi;
	struct boot_proc * bp;
	struct mmproc * mmp;
};

PRIVATE int mm_allocmem(struct exec_info * execi, int vaddr, size_t len)
{
	struct mm_exec_info * mmexeci = (struct mm_exec_info *)execi->callback_data;
	struct vir_region * vr = NULL;

	if (!(vr = mmap_region(mmexeci->mmp, vaddr, MAP_ANONYMOUS|MAP_FIXED, len, RF_WRITABLE))) return ENOMEM;
    list_add(&(vr->list), &(mmexeci->mmp->mem_regions));

    return 0;
}

PRIVATE int mm_alloctext(struct exec_info * execi, int vaddr, size_t len)
{
	struct mm_exec_info * mmexeci = (struct mm_exec_info *)execi->callback_data;
	struct vir_region * vr = NULL;

	if (!(vr = mmap_region(mmexeci->mmp, vaddr, MAP_ANONYMOUS|MAP_FIXED, len, RF_NORMAL))) return ENOMEM;
    list_add(&(vr->list), &(mmexeci->mmp->mem_regions));

    return 0;
}

PRIVATE int mm_allocstack(struct exec_info * execi, int vaddr, size_t len)
{
	struct mm_exec_info * mmexeci = (struct mm_exec_info *)execi->callback_data;
	struct vir_region * vr = NULL;

	if (!(vr = mmap_region(mmexeci->mmp, vaddr, MAP_ANONYMOUS|MAP_FIXED|MAP_GROWSDOWN, len, RF_WRITABLE))) return ENOMEM;
    list_add(&(vr->list), &(mmexeci->mmp->mem_regions));

    return 0;
}

PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;
    data_copy(execi->proc_e, (void *)vaddr, SELF, (void *)((int)(execi->header) + offset), len);
    
    return 0;
}

PRIVATE void spawn_bootproc(struct mmproc * mmp, struct boot_proc * bp)
{
	if (pgd_new(&(mmp->pgd))) panic("MM: spawn_bootproc: pgd_new failed");
	if (pgd_bind(mmp, &mmp->pgd)) panic("MM: spawn_bootproc: pgd_bind failed");

	struct mm_exec_info mmexeci;
	struct exec_info * execi = &mmexeci.execi;
    memset(&mmexeci, 0, sizeof(mmexeci));

    mmexeci.mmp = mmp;
    mmexeci.bp = bp;

    /* stack info */
    execi->stack_top = VM_STACK_TOP;
    execi->stack_size = PROC_ORIGIN_STACK;

    /* header */
    execi->header = bp->base;
    execi->header_len = bp->len;

    execi->allocmem = mm_allocmem;
    execi->allocstack = mm_allocstack;
    execi->alloctext = mm_alloctext;
    execi->copymem = read_segment;
    execi->clearproc = NULL;
    execi->clearmem = libexec_clearmem;
    execi->callback_data = &mmexeci;

    execi->proc_e = bp->endpoint;
    execi->filesize = bp->len;

    if (libexec_load_elf(execi) != 0) panic("can't load boot proc #%d", bp->endpoint);

    /* copy the stack */
    //char * orig_stack = (char*)(VM_STACK_TOP - module_stack_len);
    //data_copy(target, orig_stack, SELF, module_stp, module_stack_len);

    struct ps_strings ps;
    ps.ps_nargvstr = 0;
    //ps.ps_argvstr = orig_stack;
    //ps.ps_envstr = module_envp;

    if (kernel_exec(bp->endpoint, VM_STACK_TOP, bp->name, execi->entry_point, &ps) != 0) panic("kernel exec failed");

    vmctl(VMCTL_BOOTINHIBIT_CLEAR, bp->endpoint);
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
