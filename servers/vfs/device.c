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
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"

int do_ioctl(void)
{
    int fd = self->msg_in.FD;
    int retval = 0;
    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (filp == NULL) return EBADF;

    struct inode* pin = filp->fd_inode;
    if (pin == NULL) {
        unlock_filp(filp);
        return EBADF;
    }

    if (filp->fd_fops->ioctl == NULL) {
        unlock_filp(filp);
        return ENOTTY;
    }

    retval = filp->fd_fops->ioctl(pin, filp, self->msg_in.REQUEST,
                                  (unsigned long)self->msg_in.BUF, fproc);

    unlock_filp(filp);

    return retval;
}
