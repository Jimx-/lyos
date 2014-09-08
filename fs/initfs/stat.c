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
#include <sys/stat.h>

PUBLIC int initfs_stat(MESSAGE * p)
{
    dev_t dev = (dev_t)p->STDEV;
    ino_t num = (ino_t)p->STINO;
    int src = p->STSRC;
    char * buf = p->STBUF;

    struct stat sbuf;
    memset(&sbuf, 0, sizeof(struct stat));

    char header[512];
    initfs_rw_dev(DEV_READ, dev, initfs_headers[num], 512, header);
    struct posix_tar_header * phdr = (struct posix_tar_header *)header;

    /* fill in the information */
    sbuf.st_dev = dev;
    sbuf.st_ino = num;
    sbuf.st_mode = initfs_getmode(phdr);
    sbuf.st_nlink = 0;
    sbuf.st_uid = initfs_get8(phdr->uid);
    sbuf.st_gid = initfs_get8(phdr->gid);
    int major = initfs_get8(phdr->devmajor);
    int minor = initfs_get8(phdr->devminor);
    sbuf.st_rdev = MAKE_DEV(major, minor);
    sbuf.st_size = (off_t)initfs_getsize(phdr->size);

    sbuf.st_atime = 0;
    sbuf.st_mtime = 0;
    sbuf.st_ctime = 0;
    sbuf.st_blksize = 0;
    sbuf.st_blocks = 0;

    /* copy the information */
    data_copy(src, D, buf, getpid(), D, &sbuf, sizeof(struct stat));
    //phys_copy(va2pa(src, buf), va2pa(getpid(), &sbuf), sizeof(struct stat));

    return 0;
}
