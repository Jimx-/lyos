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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/interrupt.h>
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include <lyos/cpulocals.h>

#if CONFIG_PROFILING

PUBLIC int arch_init_profile_clock(u32 freq);
PUBLIC void arch_stop_profile_clock();
PUBLIC void arch_ack_profile_clock();

PRIVATE irq_hook_t profile_clock_irq_hook;

PRIVATE void profile_record_sample(struct proc* p, void* pc)
{
    struct kprof_sample* s = (struct kprof_sample*) (profile_sample_buf + kprof_info.mem_used);

    s->endpt = p->endpoint;
    s->pc = pc;

    kprof_info.mem_used += sizeof(struct kprof_sample);
}

PRIVATE void profile_record_proc(struct proc* p)
{
    struct kprof_proc* s = (struct kprof_proc*) (profile_sample_buf + kprof_info.mem_used);

    s->endpt = p->endpoint;
    strcpy(s->name, p->name);
    p->flags |= PF_PROFILE_RECORDED;

    kprof_info.mem_used += sizeof(struct kprof_proc);
}

PRIVATE void profile_sample(struct proc* p, void* pc)
{
    if (!kprofiling) return;
    if (kprof_info.mem_used + sizeof(struct kprof_sample) + sizeof(struct kprof_proc) > KPROF_SAMPLE_BUFSIZE) {
        return;
    }

    lock_proc(p);
    if (p == &get_cpulocal_var(idle_proc)) {
        kprof_info.idle_samples++;
    } else if ((p->priv && (p->priv->flags & PRF_PRIV_PROC)) || p->endpoint == KERNEL) {
        kprof_info.system_samples++;

        if (!(p->flags & PF_PROFILE_RECORDED)) {
            profile_record_proc(p);
        }

        profile_record_sample(p, pc);
    } else {
        kprof_info.user_samples++;
    }
    kprof_info.total_samples++;
    
    unlock_proc(p);
}

PRIVATE int profile_clock_handler(irq_hook_t *hook)
{
    struct proc* p = get_cpulocal_var(proc_ptr);

    profile_sample(p, 
#ifdef __i386__
        (void*) p->regs.eip
#elif defined(__riscv)
        0
#endif
    );

    arch_ack_profile_clock();

    return 1;
}

PUBLIC void init_profile_clock(u32 freq)
{
    int irq = arch_init_profile_clock(freq);
    if (irq >= 0) {
        put_irq_handler(irq, &profile_clock_irq_hook, profile_clock_handler);
        enable_irq(&profile_clock_irq_hook);
    }
}

PUBLIC void stop_profile_clock()
{
    arch_stop_profile_clock();
    disable_irq(&profile_clock_irq_hook);
    rm_irq_handler(&profile_clock_irq_hook);
}

#endif
