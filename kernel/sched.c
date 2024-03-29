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
#include <errno.h>
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/page.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/time.h>

u32 rr_interval_ms;

static DEFINE_CPULOCAL(struct list_head[SCHED_QUEUES], run_queues);
static DEFINE_CPULOCAL(bitchunk_t[BITCHUNKS(SCHED_QUEUES)], run_queue_bitmap);

#define JIFFIES_MS(j) (((j)*MSEC_PER_SEC) / system_hz)

void init_sched()
{
    int i, cpu;
    struct list_head* runqs;

    for (cpu = 0; cpu < CONFIG_SMP_MAX_CPUS; cpu++) {
        runqs = get_cpu_var(cpu, run_queues);
        for (i = 0; i < SCHED_QUEUES; i++)
            INIT_LIST_HEAD(&runqs[i]);
    }

    rr_interval_ms = RR_INTERVAL_DEFAULT;
}

/*****************************************************************************
 *                                pick_proc
 *****************************************************************************/
/**
 * <Ring 0> Choose one proc to run.
 *
 *****************************************************************************/
struct proc* pick_proc()
{
    struct proc *p, *selected = NULL;
    int q = -1, i, j;
    u64 smallest = -1;
    struct list_head* runqs = get_cpulocal_var(run_queues);
    bitchunk_t* bitmap = get_cpulocal_var(run_queue_bitmap);

    for (i = 0; i < BITCHUNKS(SCHED_QUEUES); i++) {
        if (bitmap[i]) {
            for (j = 0; j < BITCHUNK_BITS; j++) {
                if (bitmap[i] & (1UL << j)) {
                    q = i * BITCHUNK_BITS + j;
                    break;
                }
            }
            break;
        }
    }

    if (q == -1) {
        return NULL;
    }

    list_for_each_entry(p, &runqs[q], run_list)
    {
        if (p->deadline < JIFFIES_MS(jiffies)) {
            selected = p;
            break;
        }

        if (smallest == -1 || smallest > p->deadline) {
            smallest = p->deadline;
            selected = p;
        }
    }

    if (selected) dequeue_proc(selected);

    return selected;
}

static inline int proc_queue(struct proc* p)
{
    int queue;

    if (p->sched_policy == SCHED_RUNTIME)
        queue = p->priority;
    else
        queue = SCHED_RUNTIME_QUEUES - 2 + p->sched_policy;

    return queue;
}

/**
 * <Ring 0> Insert a process into scheduling queue.
 */
void enqueue_proc(struct proc* p)
{
    int queue = proc_queue(p);
    struct list_head* runqs = get_cpulocal_var(run_queues);
    bitchunk_t* bitmap = get_cpulocal_var(run_queue_bitmap);

    list_add(&p->run_list, &runqs[queue]);
    SET_BIT(bitmap, queue);
}

void dequeue_proc(struct proc* p)
{
    int queue = proc_queue(p);
    struct list_head* runqs = get_cpulocal_var(run_queues);
    bitchunk_t* bitmap = get_cpulocal_var(run_queue_bitmap);

    list_del(&(p->run_list));

    if (list_empty(&runqs[queue])) UNSET_BIT(bitmap, queue);
}

/**
 * <Ring 0> Called when a process has run out its counter.
 */
void proc_no_time(struct proc* p)
{
    int prio_ratio = p->priority;

    p->counter_ns = rr_interval_ms * NSEC_PER_MSEC;
    p->deadline = JIFFIES_MS(jiffies) + rr_interval_ms * prio_ratio;

    PST_UNSET(p, PST_NO_QUANTUM);
}

/**
 * <Ring 0> Update deadline of the process upon clock interrupt.
 */
void sched_clock(struct proc* p)
{
    int prio_ratio = p->priority;
    // u32 time_slice = (u32)p->counter_ns;

    // p->deadline = JIFFIES_MS(jiffies) + time_slice / NSEC_PER_MSEC *
    // prio_ratio;
    p->deadline = JIFFIES_MS(jiffies) + rr_interval_ms * prio_ratio;
}
