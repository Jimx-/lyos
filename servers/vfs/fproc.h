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

EXTERN struct fproc {
    int flags;

    uid_t realuid, effuid, suid;
    gid_t realgid, effgid, sgid;

    pid_t pid;
    endpoint_t endpoint;

    struct inode * pwd;         /* working directory */
    struct inode * root;        /* root directory */

    int umask;

    struct file_desc * filp[NR_FILES];
    pthread_mutex_t lock;
} fproc_table[NR_PROCS];

#define FPF_INUSE   0x1

PUBLIC void lock_fproc(struct fproc* fp);
PUBLIC void unlock_fproc(struct fproc* fp);

#endif  /* _VFS_FPROC_H_ */
