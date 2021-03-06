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

#include <lyos/types.h>
#include <lyos/ipc.h>
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

int booting_cpu = 0;

struct proc proc_table[NR_TASKS + NR_PROCS];
struct priv priv_table[NR_PRIV_PROCS];

/* When this is modified, also modify
 *	- NR_BOOT_PROCS in lyos/config.h
 *	- TASK_XXX in uapi/lyos/const.h
 *	- boot_priv_table in servman/global.c
 */
struct boot_proc boot_procs[NR_BOOT_PROCS] = {
    {CLOCK, "clock"},          {SYSTEM, "system"},        {KERNEL, "kernel"},
    {INTERRUPT, "interrupt"},  {TASK_MM, "MM"},           {TASK_PM, "PM"},
    {TASK_SERVMAN, "SERVMAN"}, {TASK_DEVMAN, "DEVMAN"},   {TASK_SCHED, "SCHED"},
    {TASK_FS, "VFS"},          {TASK_SYS, "SYS"},         {TASK_TTY, "TTY"},
    {TASK_RD, "RD"},           {TASK_INITFS, "INITFS"},   {TASK_SYSFS, "SYSFS"},
    {TASK_IPC, "IPC"},         {TASK_NETLINK, "NETLINK"}, {INIT, "INIT"},
};

char task_stack[STACK_SIZE_TOTAL];

irq_hook_t irq_hooks[NR_IRQ_HOOKS];

int errno;

int err_code = 0;

#if CONFIG_PROFILING
int kprofiling = 0;
#endif
