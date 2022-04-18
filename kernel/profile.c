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
#include <string.h>
#include <lyos/const.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <kernel/irq.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif

#if CONFIG_PROFILING

int arch_init_profile_clock(u32 freq);
void arch_stop_profile_clock();
void arch_ack_profile_clock();

static irq_hook_t profile_clock_irq_hook;

static void profile_record_sample(struct proc* p, void* pc)
{
    struct kprof_sample* s =
        (struct kprof_sample*)(profile_sample_buf + kprof_info.mem_used + 1);

    profile_sample_buf[kprof_info.mem_used] = KPROF_TYPE_SAMPLE;

    s->endpt = p->endpoint;
    s->pc = pc;

    kprof_info.mem_used += 1 + sizeof(struct kprof_sample);
}

static void profile_record_proc(struct proc* p)
{
    struct kprof_proc* s =
        (struct kprof_proc*)(profile_sample_buf + kprof_info.mem_used + 1);

    profile_sample_buf[kprof_info.mem_used] = KPROF_TYPE_PROC;

    s->endpt = p->endpoint;
    strlcpy(s->name, p->name, sizeof(s->name));
    s->name[sizeof(s->name) - 1] = '\0';
    p->flags |= PF_PROFILE_RECORDED;

    kprof_info.mem_used += 1 + sizeof(struct kprof_proc);
}

static void profile_sample(struct proc* p, void* pc)
{
    if (!kprofiling) return;
    if (kprof_info.mem_used + sizeof(struct kprof_sample) +
            sizeof(struct kprof_proc) + 2 >
        KPROF_SAMPLE_BUFSIZE) {
        return;
    }

    lock_proc(p);
    if (p == get_cpulocal_var_ptr(idle_proc)) {
        kprof_info.idle_samples++;
    } else if (p->priv->flags & PRF_PRIV_PROC || p->endpoint == KERNEL) {
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

static int profile_clock_handler(irq_hook_t* hook)
{
    struct proc* p = get_cpulocal_var(proc_ptr);

    profile_sample(p,
#ifdef __i386__
                   (void*)p->regs.ip
#elif defined(__x86_64)
                   (void*)p->regs.ip
#elif defined(__riscv)
                   (void*)p->regs.sepc
#elif defined(__aarch64__)
                   (void*)0
#endif
    );

    arch_ack_profile_clock();

    return 1;
}

void init_profile_clock(u32 freq)
{
    int irq = arch_init_profile_clock(freq);
    if (irq >= 0) {
        put_irq_handler(irq, &profile_clock_irq_hook, profile_clock_handler);
        enable_irq(&profile_clock_irq_hook);
    }
}

void stop_profile_clock()
{
    arch_stop_profile_clock();
    disable_irq(&profile_clock_irq_hook);
    rm_irq_handler(&profile_clock_irq_hook);
}

void nmi_profile_handler(int in_kernel, void* pc)
{
    struct proc* p = get_cpulocal_var(proc_ptr);

    if (in_kernel) {
        struct proc* kp;
        struct proc* idle = get_cpulocal_var_ptr(idle_proc);

        if (p == idle) {
            kprof_info.idle_samples++;
            kprof_info.total_samples++;
            return;
        }

        kp = endpt_proc(KERNEL);

        profile_sample(kp, pc);
    } else {
        profile_sample(p, pc);
    }
}

#endif
