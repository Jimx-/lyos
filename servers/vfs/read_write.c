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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
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
PUBLIC int request_readwrite(endpoint_t fs_ep, dev_t dev, ino_t num, u64 pos, int rw_flag, endpoint_t src,
    void * buf, int nbytes, u64 * newpos, int * bytes_rdwt)
{
    MESSAGE m;
    m.type = FS_RDWT;
    m.RWDEV = dev;
    m.RWINO = num;
    m.RWPOS = pos;
    m.RWFLAG = rw_flag;
    m.RWSRC = src;
    m.RWBUF = buf;
    m.RWCNT = nbytes;

    send_recv(BOTH, fs_ep, &m);

    if (m.type != FSREQ_RET) {
        printl("VFS: request_readwrite: received invalid message.");
        return EINVAL;
    }

    *newpos = m.RWPOS;
    *bytes_rdwt = m.RWCNT;

    return m.RWRET;
}

/**
 * <Ring 1> Perform read/wrte syscall.
 * @param  p Ptr to message.
 * @return   On success, the number of bytes read is returned. Otherwise a 
 *           negative error code is returned.
 */
PUBLIC int do_rdwt(MESSAGE * p)
{
    int fd = p->FD;
    struct file_desc * filp = pcaller->filp[fd];
    int rw_flag = p->type;
    char * buf = p->BUF;
    int src = p->source;
    int len = p->CNT;

    int bytes_rdwt = 0, retval = 0;
    u64 newpos;

    if (fd < 0) return -EBADF;
    if (!filp) return -EBADF;

    int position = filp->fd_pos;
    int flags = (mode_t)filp->fd_mode;
    struct inode * pin = filp->fd_inode;

    /* TODO: pipe goes here */
    /* if (PIPE) ... */
    if (pin == NULL) return -ENOENT;
    int file_type = pin->i_mode & I_TYPE;

    /* TODO: read/write for block special */
    if (file_type == I_CHAR_SPECIAL) {
        int t = p->type == READ ? DEV_READ : DEV_WRITE;
        p->type = t;

        int dev = pin->i_specdev;

        p->DEVICE   = MINOR(dev);
        p->BUF  = buf;
        p->CNT  = len;
        p->PROC_NR  = src;
        assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
        send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, p);
        assert(p->CNT == len);

        return p->CNT;
    } else if (file_type == I_REGULAR) {
        /* check for O_APPEND */
        if (rw_flag == WRITE) {
            if (flags & O_APPEND) position = pin->i_size;
        }
        
        /* issue the request */
        int bytes = 0;
        retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, position, rw_flag, src,
            buf, len, &newpos, &bytes);

        bytes_rdwt += bytes;
        position = newpos;
    } else {
        printl("VFS: do_rdwt: unknown file type: %x\n", file_type);
    }

    if (rw_flag == WRITE) {
        if (position > pin->i_size) pin->i_size = position;
    }

    filp->fd_pos = position;

    if (!retval) {
        return bytes_rdwt;
    }
    return retval;
}