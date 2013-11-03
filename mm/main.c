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
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "multiboot.h"
#include "page.h"
#include <elf.h>

PRIVATE int free_mem_size;
PRIVATE int paging_pages;
PRIVATE unsigned long * kernel_page_descs;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern pde_t pgd0;

PUBLIC void do_fork_test();

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
		case EXEC:
			mm_msg.RETVAL = do_exec();
			break;
		case WAIT:
			do_wait();
			reply = 0;
			break;
		case KILL:
			mm_msg.RETVAL = do_kill();
			break; 
		case RAISE:
			mm_msg.RETVAL = do_raise();
			break;
		case BRK:
			mm_msg.RETVAL = do_brk();
			break;
		case ACCT:
			mm_msg.RETVAL = do_acct();
			break;
		case GETUID:
			mm_msg.RETVAL = do_getuid();
			break;
		case SETUID:
            mm_msg.RETVAL = do_setuid();
			break;
		case GETGID:
			mm_msg.RETVAL = do_getgid();
			break;
		case SETGID:
			mm_msg.RETVAL = do_setgid();
			break;
		case GETEUID:
			mm_msg.RETVAL = do_geteuid();
			break;
		case GETEGID:
			mm_msg.RETVAL = do_getegid();
			break;
		case SIGACTION:
			mm_msg.RETVAL = do_sigaction();
			break;
		case ALARM:
			mm_msg.RETVAL = do_alarm();
			break;
		case MALLOC:
			mm_msg.RETVAL = alloc_mem(mm_msg.CNT);
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
		u32 base_h = (u32)((mmap->addr & 0xFFFFFFFF00000000) >> 32),
			base_l = (u32)(mmap->addr & 0xFFFFFFFF);
		u32 last_h = (u32)((last_byte & 0xFFFFFFFF00000000) >> 32),
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
	printl("Virtual kernel memory layout:\n");
	printl("  .text: 0x%08x - 0x%08x  (%dkB)\n", text_start, text_end, text_len / 1024);
	printl("  .data: 0x%08x - 0x%08x  (%dkB)\n", data_start, data_end, data_len / 1024);
	printl("  .bss:  0x%08x - 0x%08x  (%dkB)\n", bss_start, bss_end, bss_len / 1024);

	printl("Initial page directory at physical address: 0x%x\n", initial_pgd);
#ifdef RAMDISK
	rd_base = (unsigned char *)RAMDISK_BASE;
	rd_length = RAMDISK_LENGTH;
#endif

	mem_start = PROCS_BASE;
	free_mem_size = memory_size - mem_start;

	/* initialize hole table */
	mem_init(mem_start, free_mem_size);
	vmem_init(mem_start + KERNEL_VMA, (unsigned int)(4 * 1024 * 1024 * 1024 - mem_start - KERNEL_VMA));
}

unsigned long get_user_pages(unsigned long len)
{
    unsigned long i, j, start;
    j = 0;
    for (i = 0; i < (paging_pages * PAGE_SIZE); i++)
    {
        if (kernel_page_descs[i] & KERNEL_USER_USED_MASK)  // used?
        {
            j = 0;
        }
        else
        {
            if (j == 0)
            {
                start = i;
            }
            j++;
            if (j >= len)
            {
                break;
            }
        }
    }
    if (j == 0)
    {
        return 0;
    }
    for (i = start; i < (start + j); i++)
    {
        kernel_page_descs[i] |= KERNEL_USER_USED_MASK;
    }
    return start * PAGE_SIZE;
}

unsigned long free_user_pages(unsigned long start, unsigned long len)
{
    unsigned long i;
    unsigned long pgstart = start / PAGE_SIZE;
    for (i = pgstart; i < (pgstart + len); i++)
    {
        kernel_page_descs[i] -= kernel_page_descs[i] & KERNEL_USER_USED_MASK;
    }
    return 0;
}

