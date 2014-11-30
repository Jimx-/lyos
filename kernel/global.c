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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
#include "lyos/compile.h"
#include "sys/utsname.h"
#include <lyos/log.h>

PUBLIC  int booting_cpu = 0;

PUBLIC	struct proc proc_table[NR_KERNTASKS + NR_TASKS + NR_PROCS];
PUBLIC  struct priv priv_table[NR_PRIVPROCS];

/* 注意下面的 TASK 的顺序要与 const.h 中对应 */
PUBLIC	struct task	task_table[NR_TASKS] = {
	/* entry        stack size        task name */
	/* -----        ----------        --------- */
	{task_mm,       STACK_SIZE_MM,    "MM"        },
	{NULL,			0,				  "PM"		  },
	{task_servman,   STACK_SIZE_SERVMAN,"SERVMAN" },
	{NULL,			0,				  "DEVMAN"    },
	{task_fs,       STACK_SIZE_FS,    "VFS"       },
	{task_sys,      STACK_SIZE_SYS,   "SYS"       },
	{task_tty,      STACK_SIZE_TTY,   "TTY"       },
	{NULL,       	0,				  "RD"        },
	{NULL,	        0,                "INITFS"	  },
	{NULL, 			0, 				  "SYSFS"	  },
};

PUBLIC	struct task	user_proc_table[NR_NATIVE_PROCS] = {
	/* entry    stack size     proc name */
	/* -----    ----------     --------- */
	{NULL,   	0,				"INIT" }
};

PUBLIC struct boot_proc boot_procs[NR_BOOT_PROCS] = {
	{INTERRUPT,		"interrupt"	},
	{TASK_MM, 		"MM"		},
	{TASK_PM,		"PM"		},
	{TASK_SERVMAN, 	"SERVMAN"	},
	{TASK_DEVMAN,	"DEVMAN"	},
	{TASK_FS, 		"VFS"		},
	{TASK_SYS, 		"SYS"		},
	{TASK_TTY,		"TTY"		},
	{TASK_RD, 		"RD"		},
	{TASK_INITFS, 	"INITFS"	},
	{TASK_SYSFS,	"SYSFS"		},
	{INIT,			"INIT"		},
};

PUBLIC	char		task_stack[STACK_SIZE_TOTAL];

PUBLIC  irq_hook_t	irq_hooks[NR_IRQ_HOOKS];

PUBLIC	system_call_t	sys_call_table[NR_SYS_CALLS] = {sys_printx,
						       					   sys_sendrec,
												   sys_datacopy,
												   sys_privctl,
												   sys_getinfo,
												   sys_vmctl,
												   sys_umap,
												   sys_portio,
												   sys_vportio,
												   sys_sportio,
												   sys_irqctl,
												   sys_fork,
												   sys_clear,
												   sys_exec};
												   
PUBLIC int errno;

PUBLIC int err_code = 0;



