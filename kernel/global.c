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
#include "lyos/config.h"
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include "errno.h"
#include <kernel/irq.h>
#include <kernel/cpumask.h>

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

irq_hook_t irq_hooks[NR_IRQ_HOOKS];

int errno;

int err_code = 0;

#if CONFIG_PROFILING
int kprofiling = 0;
#endif

#define MASK_DECLARE_1(x) [x + 1][0] = (1UL << (x))
#define MASK_DECLARE_2(x) MASK_DECLARE_1(x), MASK_DECLARE_1(x + 1)
#define MASK_DECLARE_4(x) MASK_DECLARE_2(x), MASK_DECLARE_2(x + 2)
#define MASK_DECLARE_8(x) MASK_DECLARE_4(x), MASK_DECLARE_4(x + 4)

const bitchunk_t cpu_bit_bitmap[BITCHUNK_BITS + 1]
                               [BITCHUNKS(CONFIG_SMP_MAX_CPUS)] = {

                                   MASK_DECLARE_8(0),  MASK_DECLARE_8(8),
                                   MASK_DECLARE_8(16), MASK_DECLARE_8(24),
#if BITCHUNKS_BITS > 32
                                   MASK_DECLARE_8(32), MASK_DECLARE_8(40),
                                   MASK_DECLARE_8(48), MASK_DECLARE_8(56),
#endif
};
