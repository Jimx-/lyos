/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __UAPI_LYOS_SCHED_H__
#define __UAPI_LYOS_SCHED_H__

/*
 * Scheduling policies
 */
#define SCHED_NORMAL 0
#define SCHED_FIFO   1
#define SCHED_RR     2
#define SCHED_BATCH  3
/* SCHED_ISO: reserved but not implemented yet */
#define SCHED_IDLE     5
#define SCHED_DEADLINE 6

/* Can be ORed in to make sure the process is reverted back to SCHED_NORMAL on
 * fork */
#define SCHED_RESET_ON_FORK 0x40000000

/*
 * For the sched_{set,get}attr() calls
 */
#define SCHED_FLAG_RESET_ON_FORK  0x01
#define SCHED_FLAG_RECLAIM        0x02
#define SCHED_FLAG_DL_OVERRUN     0x04
#define SCHED_FLAG_KEEP_POLICY    0x08
#define SCHED_FLAG_KEEP_PARAMS    0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40

#define SCHED_FLAG_KEEP_ALL (SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_KEEP_PARAMS)

#define SCHED_FLAG_UTIL_CLAMP \
    (SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX)

#define SCHED_FLAG_ALL                                                       \
    (SCHED_FLAG_RESET_ON_FORK | SCHED_FLAG_RECLAIM | SCHED_FLAG_DL_OVERRUN | \
     SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP)

#endif
