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
#include "global.h"
    
PUBLIC int do_ioctl(MESSAGE * p)
{
    int fd = p->FD;
    struct file_desc * filp = pcaller->filp[fd];
    struct inode * pin = filp->fd_inode;

    if (pin == NULL) return EBADF;

    int file_type = pin->i_mode & I_TYPE;
    if (file_type != I_CHAR_SPECIAL && file_type != I_BLOCK_SPECIAL) return ENOTTY;

    dev_t dev = pin->i_specdev;

    if (file_type == I_BLOCK_SPECIAL) {

    } else {
        MESSAGE msg_to_driver;

        msg_to_driver.type = DEV_IOCTL;
        msg_to_driver.DEVICE = MINOR(dev);
        msg_to_driver.REQUEST = p->REQUEST;
        msg_to_driver.BUF = p->BUF;
        msg_to_driver.PROC_NR = p->source;

        assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
        send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &msg_to_driver);
    }

    return 0;
}
