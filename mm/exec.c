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
#include <elf.h>
#include "fcntl.h"
#include "sys/stat.h"
#include "proto.h"
#include "region.h"
#include "const.h"
    
#define EXEC_DEBUG 1

PRIVATE int read_elf_header(Elf32_Ehdr* elf_hdr, 
	int * text_paddr, int * text_vaddr, int * text_filelen, int * text_memlen,
	int * data_paddr, int * data_vaddr, int * data_filelen, int * data_memlen,
	int * entry_point, off_t * text_offset, off_t * data_offset);

/*****************************************************************************
 *                                do_exec
 *****************************************************************************/
/**
 * Perform the exec() system call.
 * 
 * @return  Zero if successful, otherwise -1.
 *****************************************************************************/
PUBLIC int do_exec()
{
	int text_paddr, text_vaddr, text_filelen, text_memlen;
	int data_paddr, data_vaddr, data_filelen, data_memlen;
	int entry_point;
	off_t text_offset, data_offset;

	/* get parameters from the message */
	int name_len = mm_msg.NAME_LEN;	/* length of filename */
	int src = mm_msg.source;	/* caller proc nr. */
	struct proc * p = proc_table + src;
	assert(name_len < MAX_PATH);

	char pathname[MAX_PATH];
	phys_copy((void*)va2la(TASK_MM, pathname),
		  (void*)va2la(src, mm_msg.PATHNAME),
		  name_len);
	pathname[name_len] = 0;	/* terminate the string */

	/* get the file size */
	struct stat s;
	int ret = stat(pathname, &s);
	if (ret != 0) {
		printl("MM::do_exec()::stat() returns error. %s(error code: %d)\n", pathname, ret);
		return -1;
	}

	/* read the file */
	int fd = open(pathname, O_RDWR);
	if (fd == -1)
		return -1;
	//assert(s.st_size < MMBUF_SIZE);
	read(fd, mmbuf, s.st_size);
	close(fd);

	Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*)(mmbuf);
	int retval = read_elf_header(elf_hdr, &text_paddr, &text_vaddr, &text_filelen, &text_memlen,
		&data_paddr, &data_vaddr, &data_filelen, &data_memlen,
		&entry_point, &text_offset, &data_offset);

	if (retval) return retval;

	proc_new(p, (void *)text_vaddr, text_memlen, (void *)data_vaddr, data_memlen);

	data_copy(src, D, (void *)text_vaddr, getpid(), D, (void *)((int)mmbuf + text_offset), text_filelen);
	data_copy(src, D, (void *)data_vaddr, getpid(), D, (void *)((int)mmbuf + data_offset), data_filelen);

	int orig_stack_len = mm_msg.BUF_LEN;
	char stackcopy[PROC_ORIGIN_STACK];
	data_copy(TASK_MM, D, stackcopy,
		  src, D, mm_msg.BUF,
		  orig_stack_len);

	u8 * orig_stack = (u8*)(VM_STACK_TOP - orig_stack_len);

	int delta = (int)orig_stack - (int)mm_msg.BUF;

	int argc = 0;
	u8 * envp = orig_stack;
	if (orig_stack_len) {	/* has args */
		char **q = (char**)stackcopy;
		for (; *q != 0; q++, argc++)
			*q += delta;
		q++;
		envp += (int)q - (int)stackcopy;
		for (; *q != 0; q++)
			*q += delta;
	}

	data_copy(src, D, orig_stack, TASK_MM, D, stackcopy, orig_stack_len);

	proc_table[src].regs.ecx = (u32)envp; 
	proc_table[src].regs.edx = (u32)orig_stack; 
	proc_table[src].regs.eax = argc;
	/* setup eip & esp */
	proc_table[src].regs.eip = entry_point; /* @see _start.asm */
	proc_table[src].regs.esp = (u32)orig_stack;

	strcpy(proc_table[src].name, pathname);
	
	return 0;
}

/**
 * <Ring 1> Read ELF header.
 */
PRIVATE int read_elf_header(Elf32_Ehdr* elf_hdr, 
	int * text_paddr, int * text_vaddr, int * text_filelen, int * text_memlen,
	int * data_paddr, int * data_vaddr, int * data_filelen, int * data_memlen,
	int * entry_point, off_t * text_offset, off_t * data_offset)
{
	int i;
	if (elf_hdr->e_ident[0] != 0x7f || elf_hdr->e_type != ET_EXEC) {
		printl("MM: Invalid executable format.\n");
		return ENOEXEC;
	}

#if EXEC_DEBUG
	printl("Program header entry num (phnum): %d\n", elf_hdr->e_phnum);
	printl("Entry point: 0x%x\n", elf_hdr->e_entry);
#endif

	*entry_point = elf_hdr->e_entry;

	for (i = 0; i < elf_hdr->e_phnum; i++) {
		Elf32_Phdr* prog_hdr = (Elf32_Phdr*)(mmbuf + elf_hdr->e_phoff +
			 			(i * elf_hdr->e_phentsize));
		if (prog_hdr->p_type == PT_LOAD) {
			if (prog_hdr->p_memsz == 0) continue;

			/* Text */
			if (elf_hdr->e_entry >= prog_hdr->p_vaddr && 
				elf_hdr->e_entry < (prog_hdr->p_vaddr + prog_hdr->p_memsz)) {
				*text_paddr = prog_hdr->p_paddr;
				*text_vaddr = prog_hdr->p_vaddr;
				*text_filelen = prog_hdr->p_filesz;
				*text_memlen = prog_hdr->p_memsz;
				*text_offset = prog_hdr->p_offset;
			} else {
				*data_paddr = prog_hdr->p_paddr;
				*data_vaddr = prog_hdr->p_vaddr;
				*data_filelen = prog_hdr->p_filesz;
				*data_memlen = prog_hdr->p_memsz;
				*data_offset = prog_hdr->p_offset;
			}
		}
	}

#if EXEC_DEBUG
	printl("Text: Address: v: 0x%x, p: 0x%x, Length: mem: %d, file: %d, Offset: 0x%x\n",
		*text_vaddr, *text_paddr, *text_memlen, *text_filelen, *text_offset);
	printl("Data: Address: v: 0x%x, p: 0x%x, Length: mem: %d, file: %d, Offset: 0x%x\n",
		*data_vaddr, *data_paddr, *data_memlen, *data_filelen, *data_offset);
	printl("Initial PC: 0x%x\n", *entry_point);
#endif

	return 0;
}

/**
 * <Ring 1> Create process with text and data segments.
 * @param  text_vaddr  Text base.
 * @param  text_memlen Text length.
 * @param  data_vaddr  Data base.
 * @param  data_memlen Data length.
 * @return             Zero on success.
 */
PUBLIC int proc_new(struct proc * p, void * text_vaddr, int text_memlen, void * data_vaddr, int data_memlen)
{
	/* create memory regions */
	INIT_LIST_HEAD(&(p->mem_regions));
	struct vir_region * text_region = region_new(p, text_vaddr, text_memlen, RF_SHARABLE);
	list_add(&(text_region->list), &(p->mem_regions));
	struct vir_region * data_region = region_new(p, data_vaddr, data_memlen, RF_NORMAL);
	list_add(&(data_region->list), &(p->mem_regions));
	struct vir_region * stack_region = region_new(p, VM_STACK_TOP - PROC_ORIGIN_STACK, PROC_ORIGIN_STACK, RF_NORMAL);
	list_add(&(stack_region->list), &(p->mem_regions));
	region_alloc_phys(stack_region);
	region_map_phys(p, stack_region);

	/* stack guard, see @const.h */
	struct vir_region * stack_guard_region = region_new(p, VM_STACK_TOP - PROC_ORIGIN_STACK - STACK_GUARD_LEN, STACK_GUARD_LEN, RF_GUARD);
	list_add(&(stack_guard_region->list), &(p->mem_regions));

	/* allocate physical memory */
	region_alloc_phys(text_region);
	region_alloc_phys(data_region);

	/* map the region in p's address space */
	region_map_phys(p, text_region);
	region_map_phys(p, data_region);
	
	p->brk = (int)(data_vaddr) + data_memlen + 1;

	return 0;
}
