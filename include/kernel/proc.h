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

#ifndef _PROC_H_
#define _PROC_H_

#include <lyos/const.h>
#include <signal.h>
#include <lyos/list.h>
#include <asm/stackframe.h>
#include <asm/page.h>
#include <asm/fpu.h>
#include <lyos/spinlock.h>
#include <lyos/endpoint.h>
#include <asm/cpulocals.h>

/* Process State */
#define PST_BOOTINHIBIT \
    0x01 /* this proc is not runnable until SERVMAN has made it */
#define PST_SENDING    0x02 /* set when proc trying to send */
#define PST_RECEIVING  0x04 /* set when proc trying to recv */
#define PST_NO_PRIV    0x08 /* this proc has no priv structure */
#define PST_NO_QUANTUM 0x10 /* set when proc has run out of quantum */
#define PST_PAGEFAULT  0x20 /* set when proc has encounted a page fault */
#define PST_MMINHIBIT                                                          \
    0x40 /* this proc is not runnable until MM has prepared mem regions for it \
          */
#define PST_STOPPED 0x80 /* set when proc is stopped */
#define PST_FREE_SLOT                                                   \
    0x100                      /* set when proc table entry is not used \
                                * (ok to allocated to a new process)    \
                                */
#define PST_SIGNALED    0x200  /* set when proc is signaled */
#define PST_SIG_PENDING 0x400  /* set when proc has a pending signal */
#define PST_MMREQUEST   0x800  /* set when proc issued an mm request */
#define PST_TRACED      0x1000 /* set when proc is stopped for tracing */

#define PF_RESUME_SYSCALL \
    0x001 /* set when we have to resume syscall execution */
#define PF_TRACE_SYSCALL    0x002 /* syscall tracing */
#define PF_LEAVE_SYSCALL    0x004 /* syscall tracing: leaving syscall */
#define PF_PROFILE_RECORDED 0x008 /* recorded in the sample buffer */
#define PF_RECV_ASYNC       0x010 /* proc can receive async message */
#define PF_FPU_INITIALIZED  0x020 /* proc has used FPU */
#define PF_DELIVER_MSG      0x040 /* copy message before resuming */
#define PF_MSG_FAILED       0x080 /* failed to deliver message */

#define proc_slot(n)    ((n) + NR_TASKS)
#define proc_addr(n)    (&proc_table[(n) + NR_TASKS])
#define is_kerntaske(e) (e < 0)

#define pst_is_runnable(pst)   ((pst) == 0)
#define proc_is_runnable(proc) (pst_is_runnable((proc)->state))

#define PST_IS_SET(proc, pst) ((proc)->state & pst)
#define PST_SET_LOCKED(proc, pst)                                 \
    do {                                                          \
        int prev_pst = (proc)->state;                             \
        (proc)->state |= (pst);                                   \
        if (pst_is_runnable(prev_pst) && !proc_is_runnable(proc)) \
            dequeue_proc(proc);                                   \
    } while (0)
#define PST_SET(proc, pst)         \
    do {                           \
        lock_proc(proc);           \
        PST_SET_LOCKED(proc, pst); \
        unlock_proc(proc);         \
    } while (0)
#define PST_SETFLAGS_LOCKED(proc, flags)                          \
    do {                                                          \
        int prev_pst = (proc)->state;                             \
        (proc)->state = (flags);                                  \
        if (pst_is_runnable(prev_pst) && !proc_is_runnable(proc)) \
            dequeue_proc(proc);                                   \
    } while (0)
#define PST_SETFLAGS(proc, flags)         \
    do {                                  \
        lock_proc(proc);                  \
        PST_SETFLAGS_LOCKED(proc, flags); \
        unlock_proc(proc);                \
    } while (0)
#define PST_UNSET_LOCKED(proc, pst)                               \
    do {                                                          \
        int prev_pst = (proc)->state;                             \
        (proc)->state &= ~(pst);                                  \
        if (!pst_is_runnable(prev_pst) && proc_is_runnable(proc)) \
            enqueue_proc(proc);                                   \
    } while (0)
#define PST_UNSET(proc, pst)         \
    do {                             \
        lock_proc(proc);             \
        PST_UNSET_LOCKED(proc, pst); \
        unlock_proc(proc);           \
    } while (0)

/* scheduling policy */
#define SCHED_RUNTIME  1
#define SCHED_ISO      2
#define SCHED_NORMAL   3
#define SCHED_IDLEPRIO 4

#define SCHED_RUNTIME_QUEUES 100
#define SCHED_QUEUES         103
#define SCHED_QUEUE_ISO      (SCHED_RUNTIME_QUEUES - 2 + SCHED_ISO)
#define SCHED_QUEUE_NORMAL   (SCHED_RUNTIME_QUEUES - 2 + SCHED_NORMAL)
#define SCHED_QUEUE_IDLEPRIO (SCHED_RUNTIME_QUEUES - 2 + SCHED_IDLEPRIO)

#define RR_INTERVAL_DEFAULT 6

struct proc {
    struct stackframe regs; /* process registers saved in stack frame */
    struct segframe seg;

    spinlock_t lock;

    int priority;
    u64 counter_ns; /* remained ticks */
    int quantum_ms;
    u64 cycles;

    int sched_policy;
    u64 deadline;
    struct list_head run_list;

    struct priv* priv;
    endpoint_t endpoint;
    int slot;
    char name[PROC_NAME_LEN]; /* name of the process */

    int state; /**
                * process flags.
                * A proc is runnable if state==0
                */
    int flags;
#if CONFIG_SMP
    int cpu; /* on which cpu this proc runs */
    int last_cpu;
#endif

    MESSAGE send_msg;
    MESSAGE deliver_msg;
    MESSAGE* recv_msg;
    MESSAGE* syscall_msg;
    int recvfrom;
    int sendto;

    struct proc* q_sending;    /**
                                * queue of procs sending messages to
                                * this proc
                                */
    struct proc* next_sending; /**
                                * next proc in the sending
                                * queue (q_sending)
                                */

    int p_parent; /**< pid of parent process */

    sigset_t sig_pending;

    clock_t user_time; /* user time in ticks */
    clock_t sys_time;  /* sys time in ticks */

    struct {
        struct proc* next_request;

        endpoint_t target;

        MESSAGE saved_reqmsg;

        int req_type;
        int type;
        struct {
            struct {
                void* start;
                size_t len;
                int write;
            } check;
        } params;

        int result;
    } mm_request;
};

extern struct proc proc_table[];

#define FIRST_PROC proc_table[0]
#define LAST_PROC  proc_table[NR_TASKS + NR_PROCS - 1]

DECLARE_CPULOCAL(struct proc*, proc_ptr);
DECLARE_CPULOCAL(struct proc*, pt_proc);
DECLARE_CPULOCAL(struct proc, idle_proc);
DECLARE_CPULOCAL(struct proc*, fpu_owner);

DECLARE_CPULOCAL(volatile int, cpu_is_idle);

static inline void lock_proc(struct proc* p) { spinlock_lock(&p->lock); }
static inline void unlock_proc(struct proc* p) { spinlock_lock(&p->lock); }

#endif
