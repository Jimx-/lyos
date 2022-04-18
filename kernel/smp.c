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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <lyos/smp.h>
#include <asm/proto.h>
#include <lyos/spinlock.h>

int booted_aps = 0;

static struct cpu cpus[CONFIG_SMP_MAX_CPUS];
static DEF_SPINLOCK(cpus_lock);

void wait_for_aps_to_finish_booting()
{
    int n = 0;
    int i;

    for (i = 0; i < ncpus; i++) {
        if (test_cpu_flag(i, CPU_IS_READY)) n++;
    }

    if (n != ncpus) printk("smp: only %d out of %d cpus booted\n", n, ncpus);

    while (booted_aps != (n - 1))
        arch_pause();
}

void ap_finished_booting()
{
    spinlock_lock(&cpus_lock);
    booted_aps++;
    spinlock_unlock(&cpus_lock);
}

void set_cpu_flag(int cpu, u32 flag)
{
    spinlock_lock(&cpus_lock);
    cpus[cpu].flags |= flag;
    spinlock_unlock(&cpus_lock);
}

void clear_cpu_flag(int cpu, u32 flag)
{
    spinlock_lock(&cpus_lock);
    cpus[cpu].flags &= ~flag;
    spinlock_unlock(&cpus_lock);
}

int test_cpu_flag(int cpu, u32 flag) { return cpus[cpu].flags & flag; }
