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
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "errno.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"

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

    if (!filp) return -EBADF;

    int position = filp->fd_pos;
    int flags = (mode_t)filp->fd_mode;
    struct inode * pin = filp->fd_inode;

    /* TODO: pipe goes here */
    /* if (PIPE) ... */

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
        if (rw_flag == READ) {
            if (flags & O_APPEND) position = pin->i_size;
        }

        /* issue the request */
        
    }
    return 0;
}