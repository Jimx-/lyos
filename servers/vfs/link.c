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
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include <sys/syslimits.h>

#include "global.h"

static int request_ftrunc(endpoint_t fs_ep, dev_t dev, ino_t num, int newsize);

static ssize_t request_rdlink(endpoint_t fs_ep, dev_t dev, ino_t num,
                              endpoint_t src, void* buf, size_t nbytes)
{
    MESSAGE m;
    m.type = FS_RDLINK;
    m.RWDEV = dev;
    m.RWINO = num;
    m.RWSRC = src;
    m.RWBUF = buf;
    m.RWCNT = nbytes;

    fs_sendrec(fs_ep, &m);

    if (m.type != FSREQ_RET) {
        return -EINVAL;
    }

    if (m.RWRET == 0) {
        return m.RWCNT;
    }

    return -m.RWRET;
}

int do_rdlink(void)
{
    struct lookup lookup;
    char pathname[MAX_PATH];
    struct vfs_mount* vmnt;
    struct inode* pin;
    void* buf = self->msg_in.BUF;
    size_t size = self->msg_in.BUF_LEN;
    ssize_t retval;

    if (self->msg_in.NAME_LEN >= MAX_PATH) return -ENAMETOOLONG;

    /* fetch the name */
    data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME,
              self->msg_in.NAME_LEN);
    pathname[self->msg_in.NAME_LEN] = '\0';

    init_lookup(&lookup, pathname, LKF_SYMLINK, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_READ;
    pin = resolve_path(&lookup, fproc);

    if (!pin) {
        return -err_code;
    }

    if (!S_ISLNK(pin->i_mode)) {
        retval = -EINVAL;
    } else {
        retval = request_rdlink(pin->i_fs_ep, pin->i_dev, pin->i_num,
                                self->msg_in.source, buf, size);
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

/**
 * Send FTRUNC request to FS driver.
 * @param  fs_ep   Endpoint of the FS driver.
 * @param  dev     The device number.
 * @param  num     The inode number.
 * @param  newsize New size.
 * @return         Zero on success.
 */
static int request_ftrunc(endpoint_t fs_ep, dev_t dev, ino_t num, int newsize)
{
    MESSAGE m;

    m.type = FS_FTRUNC;
    m.REQ_DEV = (int)dev;
    m.REQ_NUM = (int)num;
    m.REQ_FILESIZE = newsize;

    fs_sendrec(fs_ep, &m);

    return m.RET_RETVAL;
}

int truncate_node(struct inode* pin, int newsize)
{
    int file_type = pin->i_mode & I_TYPE;
    if (file_type != I_REGULAR) return EINVAL;
    int retval = request_ftrunc(pin->i_fs_ep, pin->i_dev, pin->i_num, newsize);
    if (retval == 0) {
        pin->i_size = newsize;
    }
    return retval;
}

static int request_symlink(endpoint_t fs_ep, dev_t dev, ino_t dir_ino,
                           char* last_component, endpoint_t src, void* target,
                           size_t target_len, struct fproc* fp)
{
    MESSAGE m;

    m.type = FS_SYMLINK;
    m.u.m_vfs_fs_symlink.dev = dev;
    m.u.m_vfs_fs_symlink.dir_ino = dir_ino;
    m.u.m_vfs_fs_symlink.name = last_component;
    m.u.m_vfs_fs_symlink.name_len = strlen(last_component);
    m.u.m_vfs_fs_symlink.src = src;
    m.u.m_vfs_fs_symlink.target = target;
    m.u.m_vfs_fs_symlink.target_len = target_len;
    m.u.m_vfs_fs_symlink.uid = fp->effuid;
    m.u.m_vfs_fs_symlink.gid = fp->effgid;

    fs_sendrec(fs_ep, &m);

    return m.RET_RETVAL;
}

int do_symlink(void)
{
    char pathname[PATH_MAX + 1];
    void *oldpath, *newpath;
    size_t oldpath_len, newpath_len;
    struct lookup lookup;
    struct inode* pin;
    struct vfs_mount* vmnt;
    int retval;

    oldpath = self->msg_in.u.m_vfs_link.old_path;
    newpath = self->msg_in.u.m_vfs_link.new_path;
    oldpath_len = self->msg_in.u.m_vfs_link.old_path_len;
    newpath_len = self->msg_in.u.m_vfs_link.new_path_len;

    if (oldpath_len == 0) {
        return ENOENT;
    } else if (oldpath_len > PATH_MAX) {
        return ENAMETOOLONG;
    }

    if (newpath_len > PATH_MAX) {
        return ENAMETOOLONG;
    }

    data_copy(SELF, pathname, fproc->endpoint, newpath, newpath_len);
    pathname[newpath_len] = 0;

    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;
    if ((pin = last_dir(&lookup, fproc)) == NULL) {
        return err_code;
    }

    retval = forbidden(fproc, pin, W_BIT | X_BIT);
    if (retval == 0) {
        retval = request_symlink(pin->i_fs_ep, pin->i_dev, pin->i_num, pathname,
                                 fproc->endpoint, oldpath, oldpath_len, fproc);
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}
