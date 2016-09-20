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

PUBLIC void __lyos_init();

PRIVATE void init_mm();
PRIVATE struct mmproc * init_mmproc(endpoint_t endpoint);
PRIVATE void spawn_bootproc(struct mmproc * mmp, struct boot_proc * bp);
PRIVATE void print_memmap();

PRIVATE void process_system_notify();

/*****************************************************************************
 *                                task_mm
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK MM.
 * 
 *****************************************************************************/
PUBLIC int main()
{
	init_mm();
	
	while (TRUE) {
		send_recv(RECEIVE, ANY, &mm_msg);
		int src = mm_msg.source;
		int reply = 1;

		int msgtype = mm_msg.type;

		switch (msgtype) {
		case NOTIFY_MSG:
            if (src == SYSTEM) 
                process_system_notify();
            break;
		case PM_MM_FORK:
			mm_msg.RETVAL = do_fork();
			break;
		case BRK:
			mm_msg.RETVAL = do_brk();
			break;
		case MMAP:
			mm_msg.RETVAL = do_mmap();
			break;
		case MUNMAP:
			mm_msg.RETVAL = do_munmap();
			break;
		case FS_MMAP:
			mm_msg.RETVAL = do_vfs_mmap();
			break;
		case PROCCTL:
			mm_msg.RETVAL = do_procctl();
			break;
		case MM_MAP_PHYS:
			mm_msg.RETVAL = do_map_phys();
			break;
		case MM_VFS_REPLY:
			mm_msg.RETVAL = do_vfs_reply();
			break;
		case MM_GETINFO:
			mm_msg.RETVAL = do_mm_getinfo();
			break;
		case FUTEX:
			mm_msg.RETVAL = do_futex();
			break;
		case FAULT:
			do_handle_fault();
			reply = 0;
			break;
		default:
			mm_msg.RETVAL = ENOSYS;
			break;
		}

		if (reply && mm_msg.RETVAL != SUSPEND) {
			mm_msg.type = SYSCALL_RET;
			send_recv(SEND_NONBLOCK, src, &mm_msg);
		}
	}

	return 0;
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
	vir_bytes vmalloc_start = (kernel_info.kernel_end_pde + MAX_PAGEDIR_PDES) * ARCH_BIG_PAGE_SIZE;
	vmem_init(vmalloc_start, VMALLOC_END - vmalloc_start);
	mem_info.vmalloc_total = VMALLOC_END - vmalloc_start;
	mem_info.vmalloc_used = 0;

	pt_init();
	slabs_init();
	page_cache_init();
	
	__lyos_init();

	init_mmproc(TASK_MM);
	
	futex_init();
	
	struct boot_proc * bp;
	for (i = -NR_TASKS, bp = kernel_info.boot_procs; bp < &kernel_info.boot_procs[NR_BOOT_PROCS]; bp++, i++) {
		if (bp->proc_nr < 0) continue;

		struct mmproc * mmp = init_mmproc(bp->endpoint);
		mmp->flags = MMPF_INUSE;

		if (bp->proc_nr == TASK_MM) continue;
		
		if ((mmp->mm = mm_allocate()) == NULL) panic("cannot allocate mm struct for %d", bp->endpoint);
		mm_init(mmp->mm);
		mmp->mm->slot = i;
		mmp->active_mm = mmp->mm;

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
			mmp->group_leader = mmp;
			mmp->mmap_base = ARCH_BIG_PAGE_SIZE;
			INIT_LIST_HEAD(&mmp->group_list);

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

	if (!(vr = mmap_region(mmexeci->mmp, vaddr, MAP_ANONYMOUS|MAP_FIXED|MAP_POPULATE, len, RF_WRITABLE))) return ENOMEM;
    list_add(&(vr->list), &mmexeci->mmp->active_mm->mem_regions);

    return 0;
}

PRIVATE int mm_allocmem_prealloc(struct exec_info * execi, int vaddr, size_t len)
{
	struct mm_exec_info * mmexeci = (struct mm_exec_info *)execi->callback_data;
	struct vir_region * vr = NULL;

	if (!(vr = mmap_region(mmexeci->mmp, vaddr, MAP_ANONYMOUS|MAP_FIXED, len, RF_WRITABLE))) return ENOMEM;
    list_add(&(vr->list), &mmexeci->mmp->active_mm->mem_regions);

    return 0;
}

PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
	struct mm_exec_info * mmexeci = (struct mm_exec_info *)execi->callback_data;
    if (offset + len > mmexeci->bp->len) return ENOEXEC;
    data_copy(execi->proc_e, (void *)vaddr, NO_TASK, (void *)((phys_bytes)(mmexeci->bp->base) + offset), len);
    return 0;
}

PRIVATE void spawn_bootproc(struct mmproc * mmp, struct boot_proc * bp)
{
	if (pgd_new(&mmp->mm->pgd)) panic("MM: spawn_bootproc: pgd_new failed");
	if (pgd_bind(mmp, &mmp->mm->pgd)) panic("MM: spawn_bootproc: pgd_bind failed");
	
	struct mm_exec_info mmexeci;
	struct exec_info * execi = &mmexeci.execi;
	char header[ARCH_PG_SIZE];
    memset(&mmexeci, 0, sizeof(mmexeci));

    mmexeci.mmp = mmp;
    mmexeci.bp = bp;

    /* stack info */
    execi->stack_top = VM_STACK_TOP;
    execi->stack_size = PROC_ORIGIN_STACK;

    /* header */
    data_copy(SELF, header, NO_TASK, bp->base, sizeof(header));
    execi->header = header;
    execi->header_len = sizeof(header);

    execi->allocmem = mm_allocmem;
    execi->allocmem_prealloc = mm_allocmem;
    execi->copymem = read_segment;
    execi->clearproc = NULL;
    execi->clearmem = libexec_clearmem;
    execi->callback_data = &mmexeci;

    execi->proc_e = bp->endpoint;
    execi->filesize = bp->len;

    char interp[MAX_PATH];
    if (elf_is_dynamic(execi->header, execi->header_len, interp, sizeof(interp)) > 0) {
        printl("%s\n", interp);
    }

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
	int first = 1;

	memory_size = kernel_info.memory_size;

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

		if (mmap->type == KINFO_MEMORY_AVAILABLE) {
			usable_memsize += mmap->len;
			if (first) {
				mem_init(mmap->addr, mmap->len);
				first = 0;
			} else {
				free_mem(mmap->addr, mmap->len);
			}
		}
		else 
			reserved_memsize += mmap->len;
	}

	vir_bytes text_start = kernel_info.kernel_text_start, text_end = kernel_info.kernel_text_end, text_len = text_end - text_start;
	vir_bytes data_start = kernel_info.kernel_data_start, data_end = kernel_info.kernel_data_end, data_len = data_end - data_start;
	vir_bytes bss_start = kernel_info.kernel_bss_start, bss_end = kernel_info.kernel_bss_end, bss_len = bss_end - bss_start;

	usable_memsize = usable_memsize - text_len - data_len - bss_len;
	printl("Memory: %dk/%dk available (%dk kernel code, %dk data, %dk reserved)\n", 
						usable_memsize / 1024, memory_size / 1024,
						text_len / 1024, (data_len + bss_len) / 1024,
						reserved_memsize / 1024);

	printl("Virtual kernel memory layout:\n");
	printl("  .text   : 0x%08x - 0x%08x  (%dkB)\n", text_start, text_end, text_len / 1024);
	printl("  .data   : 0x%08x - 0x%08x  (%dkB)\n", data_start, data_end, data_len / 1024);
	printl("  .bss    : 0x%08x - 0x%08x  (%dkB)\n", bss_start, bss_end, bss_len / 1024);

	mem_start = kernel_info.kernel_end_phys;
	free_mem_size = memory_size - mem_start;

	mem_info.mem_total = memory_size;
	mem_info.mem_free = free_mem_size;
}

PRIVATE void process_system_notify()
{
	sigset_t sigset = mm_msg.SIGSET;

    if (sigismember(&sigset, SIGKMEM)) {
    	do_mmrequest();
    }

    vmctl(VMCTL_CLEAR_MEMCACHE, SELF);
}
