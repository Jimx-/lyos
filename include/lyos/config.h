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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "config/autoconf.h"

#ifndef CONFIG_SMP_MAX_CPUS
#define CONFIG_SMP_MAX_CPUS 1
#endif

/* Number of tasks & processes */
#define NR_KERNTASKS    2
#define NR_TASKS        10
#define NR_PROCS        256
#define NR_PRIV_PROCS   64
#define NR_NATIVE_PROCS     1
#define NR_BOOT_PROCS   (NR_KERNTASKS + NR_TASKS + NR_NATIVE_PROCS)
    
#define NR_SCHED_QUEUES     16
#define MIN_USER_PRIO       (NR_SCHED_QUEUES - 1)   /* min user priority */
#define MAX_USER_PRIO       0                       /* max user priority */
#define USER_PRIO           ((MIN_USER_PRIO - MAX_USER_PRIO) / 2 + MAX_USER_PRIO)   /* default user priority */
#define TASK_PRIO           (USER_PRIO / 2)   /* default task priority */

#define USER_QUANTUM        15
#define TASK_QUANTUM        15

#define VPORTIO_BUF_SIZE     256

#if CONFIG_X86_LOCAL_APIC
#define NR_IRQ_HOOKS        64
#else
#define NR_IRQ_HOOKS        16
#endif

/* TTY */
#define NR_CONSOLES 6   /* consoles */
#define NR_SERIALS  3   /* serial ports */
#endif
