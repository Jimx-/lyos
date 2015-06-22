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
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/time.h>

PUBLIC struct list_head sched_queues[SCHED_QUEUES];
PUBLIC spinlock_t sched_queues_lock;
PUBLIC bitchunk_t sched_queue_bitmap[BITCHUNKS(SCHED_QUEUES)];

PUBLIC u32 rr_interval_ms;

#define JIFFIES_MS(j) (((j) * MSEC_PER_SEC) / system_hz)

PRIVATE void dequeue_proc_locked(struct proc * p);

PUBLIC void init_sched()
{
    int i = 0;
    spinlock_init(&sched_queues_lock);
    for (i = 0; i < SCHED_QUEUES; i++) 
        INIT_LIST_HEAD(&sched_queues[i]);

    rr_interval_ms = RR_INTERVAL_DEFAULT;
}

/*****************************************************************************
 *                                pick_proc
 *****************************************************************************/
/**
 * <Ring 0> Choose one proc to run.
 * 
 *****************************************************************************/
PUBLIC struct proc * pick_proc()
{
    struct proc *p, * selected;
    int q = -1, i, j;
    u64 smallest = -1;

    spinlock_lock(&sched_queues_lock);

    for (i = 0; i < BITCHUNKS(SCHED_QUEUES); i++) {
        if (sched_queue_bitmap[i]) {
            for (j = 0; j < BITCHUNK_BITS; j++) {
                if (sched_queue_bitmap[i] & (1 << j)) {
                    q = i * BITCHUNK_BITS + j;
                    break;
                }
            }
            break;
        }
    }

    if (q == -1) {
        spinlock_unlock(&sched_queues_lock);
        return NULL;
    }

    list_for_each_entry(p, &sched_queues[q], run_list) {
        if (p->deadline < JIFFIES_MS(jiffies)) {
            selected = p;
            break;
        }

        if (smallest == -1 || smallest > p->deadline) {
            smallest = p->deadline;
            selected = p;
        }
    }
    
    dequeue_proc_locked(selected);

    spinlock_unlock(&sched_queues_lock);
    return selected;
}

PRIVATE int proc_queue(struct proc * p)
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
PUBLIC void enqueue_proc(struct proc * p)
{
    spinlock_lock(&sched_queues_lock);

    int queue = proc_queue(p);

    list_add(&(p->run_list), &sched_queues[queue]);
    SET_BIT(sched_queue_bitmap, queue);

    spinlock_unlock(&sched_queues_lock);
}

PRIVATE void dequeue_proc_locked(struct proc * p)
{ 
    int queue = proc_queue(p);

    list_del(&(p->run_list));

    if (list_empty(&sched_queues[queue])) UNSET_BIT(sched_queue_bitmap, queue);
}

/**
 * <Ring 0> Remove a process from scheduling queue.
 */
PUBLIC void dequeue_proc(struct proc * p)
{  
    spinlock_lock(&sched_queues_lock);

    dequeue_proc_locked(p);

    spinlock_unlock(&sched_queues_lock);
}

/**
 * <Ring 0> Called when a process has run out its counter.
 */
PUBLIC void proc_no_time(struct proc * p)
{
    int prio_ratio = p->priority;

    p->counter_ns = rr_interval_ms * NSEC_PER_MSEC;
    p->deadline = JIFFIES_MS(jiffies) + rr_interval_ms * prio_ratio;

    PST_UNSET(p, PST_NO_QUANTUM);
}

/**
 * <Ring 0> Update deadline of the process upon clock interrupt.
 */
PUBLIC void sched_clock(struct proc* p)
{
    int prio_ratio = p->priority;
    //u32 time_slice = (u32)p->counter_ns;

    //p->deadline = JIFFIES_MS(jiffies) + time_slice / NSEC_PER_MSEC * prio_ratio;
    p->deadline = JIFFIES_MS(jiffies) + rr_interval_ms * prio_ratio;
}
