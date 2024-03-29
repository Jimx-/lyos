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

#ifndef _VFS_FPROC_H_
#define _VFS_FPROC_H_

#include <sys/types.h>
#include <sys/syslimits.h>

EXTERN struct fproc {
    int flags;

    uid_t realuid, effuid, suid;
    gid_t realgid, effgid, sgid;

    int ngroups;
    gid_t sgroups[NGROUPS_MAX];

    pid_t pid;
    endpoint_t endpoint;

    struct inode* pwd;  /* working directory */
    struct inode* root; /* root directory */

    dev_t tty;

    int umask;

    struct files_struct* files;
    mutex_t lock;

    struct wait_queue_head signalfd_wq;

    MESSAGE msg;
    void (*func)(void);
    struct worker_thread* worker;
} fproc_table[NR_PROCS];

#define FPF_INUSE    0x01
#define FPF_PENDING  0x02
#define FPF_DRV_PROC 0x04

void lock_fproc(struct fproc* fp);
void unlock_fproc(struct fproc* fp);

#endif /* _VFS_FPROC_H_ */
