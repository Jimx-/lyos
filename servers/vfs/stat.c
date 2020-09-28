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
#include <lyos/mgrant.h>

#include "global.h"

/**
 * <Ring 1> Issue the stat request.
 * @param  fs_ep Endpoint of FS driver.
 * @param  fd    File to stat.
 * @param  src   Who want to stat.
 * @param  buf   Buffer.
 * @return       Zero on success.
 */
int request_stat(endpoint_t fs_ep, dev_t dev, ino_t num, endpoint_t src,
                 char* buf)
{
    MESSAGE m;
    mgrant_id_t grant;

    grant = mgrant_set_proxy(fs_ep, src, (vir_bytes)buf, sizeof(struct stat),
                             MGF_WRITE);
    if (grant == GRANT_INVALID)
        panic("vfs: request_stat failed to create stat grant");

    m.type = FS_STAT;
    m.u.m_vfs_fs_stat.dev = dev;
    m.u.m_vfs_fs_stat.num = num;
    m.u.m_vfs_fs_stat.grant = grant;

    fs_sendrec(fs_ep, &m);

    mgrant_revoke(grant);

    return m.RETVAL;
}

static void generic_fill_stat(struct inode* pin, struct stat* stat)
{
    stat->st_dev = pin->i_dev;
    stat->st_ino = pin->i_num;
    stat->st_mode = pin->i_mode;
    stat->st_nlink = 0;
    stat->st_gid = pin->i_gid;
    stat->st_uid = pin->i_uid;
    stat->st_size = pin->i_size;
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
    int retval;
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

    if (pin->i_fs_ep != NO_TASK) {
        retval = request_stat(pin->i_fs_ep, pin->i_dev, pin->i_num,
                              fproc->endpoint, self->msg_in.BUF);
    } else {
        struct stat sbuf;
        generic_fill_stat(pin, &sbuf);
        retval = data_copy(fproc->endpoint, self->msg_in.BUF, SELF, &sbuf,
                           sizeof(sbuf));
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

int do_lstat(void)
{
    int namelen = self->msg_in.NAME_LEN + 1;
    char pathname[MAX_PATH];
    int retval;
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

    if (pin->i_fs_ep != NO_TASK) {
        retval = request_stat(pin->i_fs_ep, pin->i_dev, pin->i_num,
                              fproc->endpoint, self->msg_in.BUF);
    } else {
        struct stat sbuf;
        generic_fill_stat(pin, &sbuf);
        retval = data_copy(fproc->endpoint, self->msg_in.BUF, SELF, &sbuf,
                           sizeof(sbuf));
    }

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
    int retval;

    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return EINVAL;

    struct inode* pin = filp->fd_inode;
    if (!pin) return ENOENT;

    /* Issue the request */
    if (pin->i_fs_ep != NO_TASK) {
        retval = request_stat(pin->i_fs_ep, pin->i_dev, pin->i_num,
                              fproc->endpoint, buf);
    } else {
        struct stat sbuf;
        generic_fill_stat(pin, &sbuf);
        retval = data_copy(fproc->endpoint, buf, SELF, &sbuf, sizeof(sbuf));
    }

    unlock_filp(filp);

    return retval;
}
