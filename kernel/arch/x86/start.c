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

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern pde_t pgd0;
PUBLIC void * k_stacks;

PRIVATE int kinfo_set_param(char * buf, char * name, char * value);

/*======================================================================*
                            cstart
 *======================================================================*/
PUBLIC void cstart(struct multiboot_info *mboot, u32 mboot_magic)
{
	kinfo.memory_size = 0;
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
		kinfo.memory_size += mmap->len;
		mmap = (struct multiboot_mmap_entry *)((unsigned int)mmap + mmap->size + sizeof(unsigned int));
	}

	mb_mod_addr = (int)mboot->mods_addr + KERNEL_VMA;
	multiboot_module_t * last_mod = (multiboot_module_t *)mboot->mods_addr;
	last_mod += mb_mod_count - 1;

	phys_bytes mod_ends = (phys_bytes)last_mod->mod_end;

	/* setup kernel page table */
	initial_pgd = (pde_t *)((int)&pgd0 - KERNEL_VMA);
	phys_bytes procs_base = mod_ends & ARCH_VM_ADDR_MASK;
    int kernel_pts = procs_base / PT_MEMSIZE;
    if (procs_base % PT_MEMSIZE != 0) {
    	kernel_pts++;
    	procs_base = kernel_pts * PT_MEMSIZE;
    }
    kinfo.kernel_start_pde = ARCH_PDE(KERNEL_VMA);
	kinfo.kernel_start_phys = 0;
	kinfo.kernel_end_phys = procs_base; 

	pg_identity(initial_pgd);
	kinfo.kernel_end_pde = pg_mapkernel(initial_pgd);
	pg_load(initial_pgd);
	
	/* initial_pgd --> physical initial pgd */

	init_prot();

	mb_flags = mboot->flags;

	k_stacks = &k_stacks_start;

	sysinfo_user = NULL;
	memset(&kern_log, 0, sizeof(struct kern_log));
	spinlock_init(&kern_log.lock);

	static char cmdline[KINFO_CMDLINE_LEN];
	if (mb_flags & MULTIBOOT_INFO_CMDLINE) {
		static char var[KINFO_CMDLINE_LEN];
		static char value[KINFO_CMDLINE_LEN];

		memcpy(cmdline, (void *)mboot->cmdline, KINFO_CMDLINE_LEN);
		char * p = cmdline;
		while (*p) {
			int var_i = 0;
			int value_i = 0;
			while (*p == ' ') p++;
			if (!*p) break;
			while (*p && *p != '=' && *p != ' ' && var_i < KINFO_CMDLINE_LEN - 1) 
				var[var_i++] = *p++ ;
			var[var_i] = 0;
			if (*p++ != '=') continue;
			while (*p && *p != ' ' && value_i < KINFO_CMDLINE_LEN - 1) 
				value[value_i++] = *p++ ;
			value[value_i] = 0;
			
			kinfo_set_param(kinfo.cmdline, var, value);
		}
	}

	/* set initrd parameters */
	multiboot_module_t * initrd_mod = (multiboot_module_t *)mb_mod_addr;
    char * initrd_base = (char*)(initrd_mod->mod_start + KERNEL_VMA);
    unsigned int initrd_len = initrd_mod->mod_end - initrd_mod->mod_start;
	char initrd_param_buf[20];
	sprintf(initrd_param_buf, "0x%x", (unsigned int)initrd_base);
	kinfo_set_param(kinfo.cmdline, "initrd_base", initrd_param_buf);
	sprintf(initrd_param_buf, "%u", (unsigned int)initrd_len);
	kinfo_set_param(kinfo.cmdline, "initrd_len", initrd_param_buf);

	/* set module information */
	int i;
	multiboot_module_t * bootmod = initrd_mod + 1;
	for (i = 0; i < NR_BOOT_PROCS - NR_TASKS; i++, bootmod++) {
		kinfo.modules[i].start_addr = (phys_bytes)bootmod->mod_start;
		kinfo.modules[i].end_addr = (phys_bytes)bootmod->mod_end;
	}

	/* kernel memory layout */
	kinfo.kernel_text_start = (vir_bytes)*(&_text);
	kinfo.kernel_data_start = (vir_bytes)*(&_data);
	kinfo.kernel_bss_start = (vir_bytes)*(&_bss);
	kinfo.kernel_text_end = (vir_bytes)*(&_etext);
	kinfo.kernel_data_end = (vir_bytes)*(&_edata);
	kinfo.kernel_bss_end = (vir_bytes)*(&_ebss);
}

PUBLIC void init_arch()
{
	acpi_init();
}

PRIVATE int kinfo_set_param(char * buf, char * name, char * value)
{
	char *p = buf;
	char *bufend = buf + KINFO_CMDLINE_LEN;
	char *q;
	int namelen = strlen(name);
	int valuelen = strlen(value);

	while (*p) {
		if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
			q = p;
			while (*q) q++;
			for (q++; q < bufend; q++, p++)
				*p = *q;
			break;
		}
		while (*p++);
		p++;
	}
	
	for (p = buf; p < bufend && (*p || *(p + 1)); p++);
	if (p > buf) p++;
	
	if (p + namelen + valuelen + 3 > bufend)
		return -1;
	
	strcpy(p, name);
	p[namelen] = '=';
	strcpy(p + namelen + 1, value);
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0;
	return 0;
}
