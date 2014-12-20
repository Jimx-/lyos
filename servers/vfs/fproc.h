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
} fproc_table[NR_TASKS + NR_PROCS];

#endif  /* _VFS_FPROC_H_ */
