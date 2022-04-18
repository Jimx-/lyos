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
#include <sys/stat.h>
#include <lyos/sysutils.h>
#include "errno.h"
#include <lyos/mgrant.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

/**
 * <Ring 1> Send read/write request.
 * @param  fs_ep      Endpoint of FS driver.
 * @param  dev        On which device this file resides.
 * @param  num        Inode nr.
 * @param  pos        Where to read/write.
 * @param  rw_flag    Read or write.
 * @param  src        Who wanna read/write.
 * @param  buf        Buffer.
 * @param  nbytes     How many bytes to read/write.
 * @param  newpos     [OUT] New position.
 * @param  bytes_rdwt [OUT] How many bytes read/written.
 * @return            Zero on success. Otherwise an error code.
 */
int request_readwrite(endpoint_t fs_ep, dev_t dev, ino_t num, loff_t pos,
                      int rw_flag, endpoint_t src, const void* buf,
                      size_t nbytes, loff_t* newpos, size_t* bytes_rdwt)
{
    MESSAGE m;
    mgrant_id_t grant;
    int retval;

    grant = mgrant_set_proxy(fs_ep, src, (vir_bytes)buf, nbytes,
                             (rw_flag == READ) ? MGF_WRITE : MGF_READ);
    if (grant == GRANT_INVALID)
        panic("vfs: request_getdents failed to create proxy grant");

    m.type = FS_RDWT;
    m.u.m_vfs_fs_readwrite.dev = dev;
    m.u.m_vfs_fs_readwrite.num = num;
    m.u.m_vfs_fs_readwrite.position = pos;
    m.u.m_vfs_fs_readwrite.rw_flag = rw_flag;
    m.u.m_vfs_fs_readwrite.grant = grant;
    m.u.m_vfs_fs_readwrite.count = nbytes;

    fs_sendrec(fs_ep, &m);
    retval = m.u.m_vfs_fs_readwrite.status;

    mgrant_revoke(grant);

    if (retval == 0) {
        if (newpos) *newpos = m.u.m_vfs_fs_readwrite.position;
        if (bytes_rdwt) *bytes_rdwt = m.u.m_vfs_fs_readwrite.count;
    }

    return retval;
}

/**
 * <Ring 1> Perform read/wrte syscall.
 * @param  p Ptr to message.
 * @return   On success, the number of bytes read is returned. Otherwise a
 *           negative error code is returned.
 */
int do_rdwt(void)
{
    int fd = self->msg_in.FD;
    int rw_flag = self->msg_in.type;
    rwlock_type_t lock_type = (rw_flag == WRITE) ? RWL_WRITE : RWL_READ;
    struct file_desc* filp = get_filp(fproc, fd, lock_type);
    char* buf = self->msg_in.BUF;
    int len = self->msg_in.CNT;

    ssize_t retval = 0;

    if (!filp) return -EBADF;

    loff_t position = filp->fd_pos;
    struct inode* pin = filp->fd_inode;

    if (pin == NULL) {
        unlock_filp(filp);
        return -ENOENT;
    }

    if (S_ISDIR(pin->i_mode)) {
        retval = -EISDIR;
    } else if (filp->fd_fops == NULL) {
        retval = -EBADF;
    } else {
        retval = -EINVAL;

        if (rw_flag == READ) {
            if (filp->fd_fops->read) {
                retval = filp->fd_fops->read(filp, buf, len, &position, fproc);
            }
        } else {
            if (filp->fd_fops->write) {
                retval = filp->fd_fops->write(filp, buf, len, &position, fproc);
            }
        }

        if (retval < 0) {
            goto err;
        }

        if (rw_flag == WRITE) {
            if (position > pin->i_size) pin->i_size = position;
        }
    }

    filp->fd_pos = position;

err:
    unlock_filp(filp);
    return retval;
}

static int request_getdents(endpoint_t fs_ep, dev_t dev, ino_t num,
                            u64 position, endpoint_t src, void* buf,
                            size_t nbytes, u64* newpos)
{
    MESSAGE m;
    mgrant_id_t grant;
    int retval;

    grant = mgrant_set_proxy(fs_ep, src, (vir_bytes)buf, nbytes, MGF_WRITE);
    if (grant == GRANT_INVALID)
        panic("vfs: request_getdents failed to create proxy grant");

    m.type = FS_GETDENTS;
    m.u.m_vfs_fs_readwrite.dev = dev;
    m.u.m_vfs_fs_readwrite.num = num;
    m.u.m_vfs_fs_readwrite.position = position;
    m.u.m_vfs_fs_readwrite.grant = grant;
    m.u.m_vfs_fs_readwrite.count = nbytes;

    fs_sendrec(fs_ep, &m);
    retval = m.u.m_vfs_fs_readwrite.status;

    mgrant_revoke(grant);

    if (retval == 0) {
        *newpos = m.u.m_vfs_fs_readwrite.position;
        return m.u.m_vfs_fs_readwrite.count;
    }

    return -retval;
}

int do_getdents(void)
{
    int fd = self->msg_in.FD;
    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return -EBADF;

    if (!(filp->fd_mode & R_BIT)) {
        unlock_filp(filp);
        return -EBADF;
    }

    if (!S_ISDIR(filp->fd_inode->i_mode)) {
        unlock_filp(filp);
        return -EBADF;
    }

    u64 newpos = 0;
    int retval =
        request_getdents(filp->fd_inode->i_fs_ep, filp->fd_inode->i_dev,
                         filp->fd_inode->i_num, filp->fd_pos, fproc->endpoint,
                         self->msg_in.BUF, self->msg_in.CNT, &newpos);
    if (retval > 0) filp->fd_pos = newpos;

    unlock_filp(filp);

    return retval;
}
