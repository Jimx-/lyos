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

#include <lyos/type.h>
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

    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
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
