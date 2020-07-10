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
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include <sys/stat.h>
#include "global.h"

/**
 * <Ring 1> Issue the stat request.
 * @param  fs_ep Endpoint of FS driver.
 * @param  fd    File to stat.
 * @param  src   Who want to stat.
 * @param  buf   Buffer.
 * @return       Zero on success.
 */
int request_stat(endpoint_t fs_ep, dev_t dev, ino_t num, int src, char* buf)
{
    MESSAGE m;

    m.type = FS_STAT;
    m.STDEV = dev;
    m.STINO = num;
    m.STSRC = src;
    m.STBUF = buf;

    fs_sendrec(fs_ep, &m);

    return m.STRET;
}

/**
 * <Ring 1> Perform the stat syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success.
 */
int do_stat(void)
{
    int namelen = self->msg_in.NAME_LEN + 1;
    char pathname[MAX_PATH];
    if (namelen > MAX_PATH) return ENAMETOOLONG;

    data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME, namelen);
    pathname[namelen] = 0;

    struct lookup lookup;
    struct inode* pin = NULL;
    struct vfs_mount* vmnt = NULL;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_READ;
    pin = resolve_path(&lookup, fproc);

    if (!pin) return ENOENT;

    int retval = request_stat(pin->i_fs_ep, pin->i_dev, pin->i_num,
                              fproc->endpoint, self->msg_in.BUF);

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

int do_lstat(void)
{
    int namelen = self->msg_in.NAME_LEN + 1;
    char pathname[MAX_PATH];
    if (namelen > MAX_PATH) return ENAMETOOLONG;

    data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME, namelen);
    pathname[namelen] = 0;

    struct lookup lookup;
    struct inode* pin = NULL;
    struct vfs_mount* vmnt = NULL;

    init_lookup(&lookup, pathname, LKF_SYMLINK, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_READ;
    pin = resolve_path(&lookup, fproc);

    if (!pin) return ENOENT;

    int retval = request_stat(pin->i_fs_ep, pin->i_dev, pin->i_num,
                              fproc->endpoint, self->msg_in.BUF);

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

/**
 * <Ring 1> Perform the fstat syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success.
 */
int do_fstat(void)
{
    int fd = self->msg_in.FD;
    char* buf = self->msg_in.BUF;

    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return EINVAL;

    /* Issue the request */
    int retval = request_stat(filp->fd_inode->i_fs_ep, filp->fd_inode->i_dev,
                              filp->fd_inode->i_num, fproc->endpoint, buf);

    unlock_filp(filp);

    return retval;
}
