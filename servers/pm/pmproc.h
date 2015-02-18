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
    
/* Information of a process used in PM */
struct pmproc {
    int flags;
    
    pid_t pid;

    endpoint_t endpoint;
    endpoint_t parent;

    sigset_t sig_pending;   /* signals to be handled */
    sigset_t sig_mask;
    struct sigaction sigaction[NSIG];
    vir_bytes sigreturn_f;

    uid_t realuid, effuid, suid;
    gid_t realgid, effgid, sgid;
    pid_t procgrp;

    int exit_status;
};

#define PMPF_INUSE      0x01
#define PMPF_WAITING    0x02
#define PMPF_HANGING    0x04

#endif
