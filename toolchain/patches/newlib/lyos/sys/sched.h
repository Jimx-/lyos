/*
 *  Written by Joel Sherrill <joel@OARcorp.com>.
 *
 *  COPYRIGHT (c) 1989-2010.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  Permission to use, copy, modify, and distribute this software for any
 *  purpose without fee is hereby granted, provided that this entire notice
 *  is included in all copies of any software which is or includes a copy
 *  or modification of this software.
 *
 *  THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 *  WARRANTY.  IN PARTICULAR,  THE AUTHOR MAKES NO REPRESENTATION
 *  OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
 *  SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 *  $Id: sched.h,v 1.3 2010/04/01 18:33:37 jjohnstn Exp $
 */

#ifndef _SYS_SCHED_H_
#define _SYS_SCHED_H_

#include <sys/cdefs.h>

/* Cloning flags. */
#define CSIGNAL  0x000000ff /* Signal mask to be sent at exit.  */
#define CLONE_VM 0x00000100 /* Set if VM shared between processes.  */
#define CLONE_FS 0x00000200 /* Set if fs info shared between processes.  */
#define CLONE_FILES \
    0x00000400 /* Set if open files shared between processes.  */
#define CLONE_SIGHAND 0x00000800 /* Set if signal handlers shared.  */
#define CLONE_PIDFD                                                \
    0x00001000                  /* Set if a pidfd should be placed \
                   in parent.  */
#define CLONE_PTRACE 0x00002000 /* Set if tracing continues on the child.  */
#define CLONE_VFORK                                    \
    0x00004000 /* Set if the parent wants the child to \
  wake it up on mm_release.  */
#define CLONE_PARENT                                                \
    0x00008000                   /* Set if we want to have the same \
                    parent as the cloner.  */
#define CLONE_THREAD  0x00010000 /* Set to add to same thread group.  */
#define CLONE_NEWNS   0x00020000 /* Set to create new namespace.  */
#define CLONE_SYSVSEM 0x00040000 /* Set to shared SVID SEM_UNDO semantics.  */
#define CLONE_SETTLS  0x00080000 /* Set TLS info.  */
#define CLONE_PARENT_SETTID                     \
    0x00100000 /* Store TID in userlevel buffer \
before MM copy.  */
#define CLONE_CHILD_CLEARTID                                        \
    0x00200000                    /* Register exit futex and memory \
                 location to clear.  */
#define CLONE_DETACHED 0x00400000 /* Create clone detached.  */
#define CLONE_UNTRACED                             \
    0x00800000 /* Set if the tracing process can't \
  force CLONE_PTRACE on this clone.  */
#define CLONE_CHILD_SETTID                                             \
    0x01000000                     /* Store TID in userlevel buffer in \
                  the child.  */
#define CLONE_NEWCGROUP 0x02000000 /* New cgroup namespace.  */
#define CLONE_NEWUTS    0x04000000 /* New utsname group.  */
#define CLONE_NEWIPC    0x08000000 /* New ipcs.  */
#define CLONE_NEWUSER   0x10000000 /* New user namespace.  */
#define CLONE_NEWPID    0x20000000 /* New pid namespace.  */
#define CLONE_NEWNET    0x40000000 /* New network namespace.  */
#define CLONE_IO        0x80000000 /* Clone I/O context.  */

__BEGIN_DECLS

int clone(int (*fn)(void* arg), void* child_stack, int flags, void* arg, ...);

/* Scheduling Policies */
/* Open Group Specifications Issue 6 */
#if defined(__CYGWIN__)
#define SCHED_OTHER 3
#else
#define SCHED_OTHER 0
#endif

#define SCHED_FIFO 1
#define SCHED_RR   2

#if defined(_POSIX_SPORADIC_SERVER)
#define SCHED_SPORADIC 4
#endif

/* Scheduling Parameters */
/* Open Group Specifications Issue 6 */

struct sched_param {
    int sched_priority; /* Process execution scheduling priority */

#if defined(_POSIX_SPORADIC_SERVER) || defined(_POSIX_THREAD_SPORADIC_SERVER)
    int sched_ss_low_priority; /* Low scheduling priority for sporadic */
                               /*   server */
    struct timespec sched_ss_repl_period;
    /* Replenishment period for sporadic server */
    struct timespec sched_ss_init_budget;
    /* Initial budget for sporadic server */
    int sched_ss_max_repl; /* Maximum pending replenishments for */
                           /* sporadic server */
#endif
};

__END_DECLS

#endif
/* end of include file */
