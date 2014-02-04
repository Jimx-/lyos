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
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/list.h"
#include "proto.h"
#include "global.h"
#include "tar.h"

PRIVATE void bdev_open(dev_t dev);
PRIVATE void bdev_close(dev_t dev);

PUBLIC int initfs_readsuper(MESSAGE * p)
{
    dev_t dev = p->REQ_DEV;

    /* open the device where this superblock resides on */
    bdev_open(dev);

    char buf[512];
    int i, position = 0;
    initfs_headers_count = 0;
    for (i = 0;;i++)
    {
        initfs_rw_dev(DEV_READ, dev, position, 512, buf);

        struct posix_tar_header * header = (struct posix_tar_header *)buf;
        if (header->name[0] == '\0')
            break;

        int size = initfs_getsize(header->size);
        initfs_headers[i] = position;
        position += ((size / 512) + 1) * 512;
        if (size % 512)
            position += 512;

        initfs_headers_count++;
    }

    /* fill result */
	p->RET_NUM = 0;
	p->RET_UID = 0;
	p->RET_GID = 0;
	p->RET_FILESIZE = 0;
	p->RET_MODE = 0;
    
    return 0;
}

PRIVATE void bdev_open(dev_t dev)
{
    MESSAGE driver_msg;
    driver_msg.type = DEV_OPEN;
    driver_msg.DEVICE = MINOR(dev);
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
}

PRIVATE void bdev_close(dev_t dev)
{
    MESSAGE driver_msg;
    driver_msg.type = DEV_CLOSE;
    driver_msg.DEVICE = MINOR(dev);
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
} 
