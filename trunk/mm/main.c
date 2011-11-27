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

#include "type.h"
#include "config.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE int free_mem_size;
/*PRIVATE int paging_pages;
PRIVATE unsigned char mem_map[8192] = {0,};*/

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
 *****************************************************************************/
PRIVATE void init_mm()
{
	struct boot_params bp;
	get_boot_params(&bp);

	memory_size = bp.mem_size;
	kernel_addr = bp.kernel_file;
	
	printl("Memory:%dMB\n", memory_size / 1024 /1024);
	
	unsigned int k_base;
	unsigned int k_limit;
	int ret = get_kernel_map(&k_base, &k_limit);
	
	if (ret == 0)
		printl("Kernel memory: 0x%x - 0x%x(%dkB)\n",
					k_base, k_base + k_limit, k_limit/1024);
	
	/* int page_tbl_size = memory_size / 1024;
	buffer_base = (int)PAGE_TBL_BASE + page_tbl_size + (1024 * 1024);
	buffer_length = (2 * 1024 * 1024);
	init_buffer();
	
	rd_base = buffer_base + buffer_length + (256 * 1024);
	rd_length = 2 * 1024 * 1024;

	mem_start = rd_base + rd_length + (256 * 1024); */
	
	buffer_base = (unsigned char *)BUFFER_BASE;
	buffer_length = BUFFER_LENGTH;
	//init_buffer();
	
#ifdef RAMDISK
	rd_base = (unsigned char *)RAMDISK_BASE;
	rd_length = RAMDISK_LENGTH;
#endif

	mem_start = PROCS_BASE;
	free_mem_size = memory_size - mem_start;
	printl("Free memory:%dMB\n", free_mem_size / 1024 / 1024);

	/*paging_pages = memory_size >> 12;

	int i;
	
	for(i=0;i<paging_pages;i++)
		mem_map[i] = 100;
	
	i = mem_start >> 12;
	int j = (memory_size - mem_start)>>12;
	printl("%d/%d pages free\n", j, paging_pages);
	
	while(j-->0)
	  mem_map[i++] = 0;*/
}

/*****************************************************************************
 *                                alloc_mem
 *****************************************************************************/
/**
 * Allocate a memory block for a proc.
 * 
 * @param pid  Which proc the memory is for.
 * @param memsize  How many bytes is needed.
 * 
 * @return  The base of the memory just allocated.
 *****************************************************************************/
PUBLIC int alloc_mem(int pid, int memsize)
{
	assert(pid >= (NR_TASKS + NR_NATIVE_PROCS));
	if (memsize > PROC_IMAGE_SIZE_DEFAULT) {
		panic("unsupported memory request: %d. "
		      "(should be less than %d)",
		      memsize,
		      PROC_IMAGE_SIZE_DEFAULT);
	}

	int base = PROCS_BASE +
		(pid - (NR_TASKS + NR_NATIVE_PROCS)) * PROC_IMAGE_SIZE_DEFAULT;

	if (base + memsize >= memory_size)
		panic("memory allocation failed. pid:%d", pid);

	return base;
}

/*****************************************************************************
 *                                free_mem
 *****************************************************************************/
/**
 * Free a memory block. Because a memory block is corresponding with a PID, so
 * we don't need to really `free' anything. In another word, a memory block is
 * dedicated to one and only one PID, no matter what proc actually uses this
 * PID.
 * 
 * @param pid  Whose memory is to be freed.
 * 
 * @return  Zero if success.
 *****************************************************************************/
PUBLIC int free_mem(int pid)
{
	return 0;
}
