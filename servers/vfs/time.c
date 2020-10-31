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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"
#include "thread.h"

#define DO_UTIMENS  1
#define DO_FUTIMENS 2

static int request_utime(endpoint_t fs_ep, dev_t dev, ino_t num,
                         struct timespec* atime, struct timespec* mtime)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));
    m.type = FS_UTIME;
    m.u.m_vfs_fs_utime.dev = dev;
    m.u.m_vfs_fs_utime.num = num;
    m.u.m_vfs_fs_utime.atime = atime->tv_sec;
    m.u.m_vfs_fs_utime.ansec = atime->tv_nsec;
    m.u.m_vfs_fs_utime.mtime = mtime->tv_sec;
    m.u.m_vfs_fs_utime.mnsec = mtime->tv_nsec;

    fs_sendrec(fs_ep, &m);

    return m.RETVAL;
}

int do_utimensat(void)
{
    int fd = self->msg_in.u.m_vfs_utimensat.fd;
    int name_len = self->msg_in.u.m_vfs_utimensat.name_len;
    int flags = self->msg_in.u.m_vfs_utimensat.flags;
    struct lookup lookup;
    struct file_desc* filp;
    struct vfs_mount* vmnt;
    struct inode* pin;
    int type, lookup_flags;
    struct timespec atime, mtime, now_ts, new_atime, new_mtime;
    char pathname[PATH_MAX];
    int retval;

    atime.tv_sec = self->msg_in.u.m_vfs_utimensat.atime;
    atime.tv_nsec = self->msg_in.u.m_vfs_utimensat.ansec;
    mtime.tv_sec = self->msg_in.u.m_vfs_utimensat.mtime;
    mtime.tv_nsec = self->msg_in.u.m_vfs_utimensat.mnsec;

    if (self->msg_in.u.m_vfs_utimensat.pathname != NULL) {
        type = DO_UTIMENS;

        lookup_flags = 0;
        if (flags & AT_SYMLINK_NOFOLLOW) lookup_flags |= LKF_SYMLINK_NOFOLLOW;

        if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                                self->msg_in.u.m_vfs_utimensat.pathname,
                                name_len)) != OK)
            return retval;
        pathname[name_len] = 0;

        init_lookupat(&lookup, fd, pathname, lookup_flags, &vmnt, &pin);
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_READ;
        pin = resolve_path(&lookup, fproc);

        if (!pin) return err_code;
    } else {
        type = DO_FUTIMENS;

        if (fd == AT_FDCWD || flags & AT_SYMLINK_NOFOLLOW) return EINVAL;

        if ((filp = get_filp(fproc, fd, RWL_READ)) == NULL) return EBADF;

        pin = filp->fd_inode;
    }

    assert(pin);

    retval = 0;

    if (pin->i_uid != fproc->effuid || fproc->effuid != SU_UID) retval = EPERM;
    if (retval != OK && atime.tv_nsec == UTIME_NOW &&
        mtime.tv_nsec == UTIME_NOW)
        retval = forbidden(fproc, pin, W_BIT);
    if (pin->i_vmnt && pin->i_vmnt->m_flags & VMNT_READONLY) retval = EROFS;

    if (retval != OK) goto out;

    if (atime.tv_nsec == UTIME_NOW || atime.tv_nsec == UTIME_OMIT ||
        mtime.tv_nsec == UTIME_NOW || mtime.tv_nsec == UTIME_OMIT) {
        now_ts.tv_sec = now();
        now_ts.tv_nsec = 0;
    }

    memset(&new_atime, 0, sizeof(new_atime));
    memset(&new_mtime, 0, sizeof(new_mtime));

    switch (atime.tv_nsec) {
    case UTIME_NOW:
        new_atime = now_ts;
        break;
    case UTIME_OMIT:
        new_atime.tv_nsec = UTIME_OMIT;
        break;
    default:
        if (atime.tv_nsec >= 1000000000UL)
            retval = EINVAL;
        else
            new_atime = atime;
        break;
    }

    switch (mtime.tv_nsec) {
    case UTIME_NOW:
        new_mtime = now_ts;
        break;
    case UTIME_OMIT:
        new_mtime.tv_nsec = UTIME_OMIT;
        break;
    default:
        if (mtime.tv_nsec >= 1000000000UL)
            retval = EINVAL;
        else
            new_mtime = mtime;
        break;
    }

    if (retval == 0)
        retval = request_utime(pin->i_fs_ep, pin->i_dev, pin->i_num, &new_atime,
                               &new_mtime);

out:
    if (type == DO_UTIMENS) {
        unlock_inode(pin);
        unlock_vmnt(vmnt);
        put_inode(pin);
    } else {
        unlock_filp(filp);
    }

    return retval;
}
