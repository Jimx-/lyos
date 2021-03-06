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

#ifndef _FS_H_
#define _FS_H_

#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/list.h>
#include <sys/syslimits.h>

#define MAJOR_NONE  0
#define NR_NONEDEVS 64

#define FS_LABEL_MAX 16

/* VFS/FS flags */

/* Lookup flags */
#define LKF_SYMLINK_NOFOLLOW 0x01 /* Return a symbolic link node */
#define LKF_INLINE_UCRED     0x02 /* In-line user credentials */

/* VFS/FS error messages */
#define EENTERMOUNT (-301)
#define ELEAVEMOUNT (-302)
#define ESYMLINK    (-303)

/* Message types */
#define REQ_DEV       u.m3.m3i1
#define REQ_NUM       u.m3.m3l2
#define REQ_START_INO u.m3.m3i2
#define REQ_FILESIZE  u.m3.m3i2
#define REQ_ROOT_INO  u.m3.m3i3
#define REQ_NAMELEN   u.m3.m3i4
#define REQ_FLAGS     u.m3.m3l1
#define REQ_MODE      u.m3.m3l1
#define REQ_PATHNAME  u.m3.m3p1
#define REQ_STARTPOS  u.m3.m3i3
#define REQ_ENDPOS    u.m3.m3i4
#define REQ_UCRED     u.m3.m3p2

/* for mm request */
#define MMRTYPE      u.m3.m3i1
#define MMRRESULT    u.m3.m3i1
#define MMR_FDLOOKUP 1
#define MMR_FDREAD   2
#define MMR_FDWRITE  3
#define MMR_FDCLOSE  4
#define MMR_FDMMAP   5
#define MMRFD        u.m3.m3i2
#define MMRENDPOINT  u.m3.m3i3
#define MMRDEV       u.m3.m3i4
#define MMRINO       u.m3.m3l1
#define MMROFFSET    u.m3.m3l1
#define MMRLENGTH    u.m3.m3l2
#define MMRMODE      u.m3.m3l2
#define MMRBUF       u.m3.m3p1

#define RET_RETVAL   u.m5.m5i1
#define RET_NUM      u.m5.m5i2
#define RET_UID      u.m5.m5i3
#define RET_GID      u.m5.m5i4
#define RET_FILESIZE u.m5.m5i5
#define RET_MODE     u.m5.m5i6
#define RET_OFFSET   u.m5.m5i7
#define RET_SPECDEV  u.m5.m5i8

#define MS_READONLY 0x001
#define RF_READONLY 0x001
#define RF_ISROOT   0x002

/* User credential info */
struct vfs_ucred {
    uid_t uid;
    gid_t gid;
    int ngroups;
    gid_t sgroups[NGROUPS_MAX];
};

#define _POSIX_SYMLOOP_MAX 8

#define VFS_TXN_TYPE_ID(t, id) (((t) << 16) | ((id)&0xffff))
#define VFS_TXN_GET_ID(t)      ((t)&0xffff)
#define VFS_TXN_GET_TYPE(t)    (((t) >> 16) & 0xffff)

struct vfs_exec_request {
    endpoint_t endpoint;
    void* pathname;
    size_t name_len;
    void* frame;
    size_t frame_size;
};

struct vfs_exec_response {
    int status;
    uid_t new_uid;
    gid_t new_gid;
    void* frame;
    size_t frame_size;
};

#endif /* _FS_H_ */
