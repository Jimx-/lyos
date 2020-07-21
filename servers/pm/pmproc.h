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

#ifndef _PM_MMPROC_H_
#define _PM_MMPROC_H_

#include <signal.h>

#include "futex.h"

/* Information of a process used in PM */
struct pmproc {
    int flags;

    pid_t tgid;
    pid_t pid;

    endpoint_t endpoint;
    endpoint_t parent;
    endpoint_t tracer;

    endpoint_t wait_pid;

    sigset_t sig_pending; /* signals to be handled */
    sigset_t sig_mask;
    sigset_t sig_mask_saved;
    sigset_t sig_ignore;
    sigset_t sig_catch;
    sigset_t sig_trace;
    int sig_status;
    struct sigaction sigaction[NSIG];
    void* sigreturn_f;

    uid_t realuid, effuid, suid;
    gid_t realgid, effgid, sgid;
    pid_t procgrp;

    struct pmproc* group_leader;
    struct list_head thread_group;

    void* frame_addr;
    size_t frame_size;

    struct futex_entry futex_entry;

    int exit_status;
};

#define PMPF_INUSE        0x01
#define PMPF_WAITING      0x02
#define PMPF_HANGING      0x04
#define PMPF_SIGSUSPENDED 0x08
#define PMPF_TRACED       0x10

#endif
