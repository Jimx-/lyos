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

#define GLOBAL_VARIABLES_HERE

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "lyos/fs.h"
#include "termios.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
#include "lyos/compile.h"
#include "sys/utsname.h"
#include <lyos/log.h>

PUBLIC  int booting_cpu = 0;

PUBLIC	struct proc proc_table[NR_TASKS + NR_PROCS];

/* 注意下面的 TASK 的顺序要与 const.h 中对应 */
PUBLIC	struct task	task_table[NR_TASKS] = {
	/* entry        stack size        task name */
	/* -----        ----------        --------- */
	{task_mm,       STACK_SIZE_MM,    "MM"        },
	{task_servman,   STACK_SIZE_SERVMAN,"SERVMAN" },
	{NULL,			0,				  "DEVMAN"    },
	{task_fs,       STACK_SIZE_FS,    "VFS"       },
	{task_sys,      STACK_SIZE_SYS,   "SYS"       },
	{task_tty,      STACK_SIZE_TTY,   "TTY"       },
	{task_rd,       STACK_SIZE_RD,    "RD"        },
	{NULL,	        0,                "INITFS"	  },
	{NULL, 			0, 				  "SYSFS"	  },
	{task_hd,       STACK_SIZE_HD,    "HD"        },
	{task_ext2_fs,  STACK_SIZE_EXT2_FS,"EXT2_FS"  },
	{task_pci,      STACK_SIZE_PCI,   "PCI"       },
};

PUBLIC	struct task	user_proc_table[NR_NATIVE_PROCS] = {
	/* entry    stack size     proc name */
	/* -----    ----------     --------- */
	{NULL,   	0,				"INIT" }
};

PUBLIC	char		task_stack[STACK_SIZE_TOTAL];

PUBLIC	TTY		tty_table[NR_CONSOLES];
PUBLIC	CONSOLE		console_table[NR_CONSOLES];

PUBLIC	irq_handler	irq_table[NR_IRQ];

PUBLIC	system_call	sys_call_table[NR_SYS_CALL] = {sys_printx,
						       					   sys_sendrec,
												   sys_datacopy,
												   sys_privctl,
												   sys_getinfo,
												   sys_vmctl};
												   
PUBLIC int errno;

PUBLIC int err_code = 0;

/* FS related below */
/*****************************************************************************/
/**
 * For dd_map[k],
 * `k' is the device nr.\ dd_map[k].driver_nr is the driver nr.
 *
 * Remeber to modify include/const.h if the order is changed.
 *****************************************************************************/
struct dev_drv_map dd_map[] = {
	/* driver nr.		major device nr.
	   ----------		---------------- */
	{INVALID_DRIVER},	/**< 0 : Unused */
	{TASK_RD},			/**< 1 : Ramdisk */
	{INVALID_DRIVER},	/**< 2 : Floppy */
	{TASK_HD},			/**< 3 : Hard disk */
	{TASK_TTY},			/**< 4 : TTY */
	{INVALID_DRIVER}	/**< 5 : Scsi disk */
};

PUBLIC	u8 *		fsbuf		= (u8*)&_fsbuf;


