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
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "region.h"
#include "proto.h"
#include "const.h"
#include "global.h"

PRIVATE DEF_LIST(file_list);

PRIVATE struct mm_file_desc* alloc_mm_file_desc(int fd, dev_t dev, ino_t ino)
{
    struct mm_file_desc* filp;
    SLABALLOC(filp);
    if (!filp) return NULL;

    filp->fd = fd;
    filp->dev = dev;
    filp->ino = ino;
    filp->refcnt = 0;
    INIT_LIST_HEAD(&filp->list);
    list_add(&filp->list, &file_list);

    return filp;
}

PUBLIC struct mm_file_desc* get_mm_file_desc(int fd, dev_t dev, ino_t ino)
{
    struct mm_file_desc* filp;
    list_for_each_entry(filp, &file_list, list) {
        if (filp->dev == dev && filp->ino == ino && filp->fd == fd) return filp;
    }

    return alloc_mm_file_desc(fd, dev, ino);
}

PUBLIC void file_reference(struct vir_region* vr, struct mm_file_desc* filp)
{
    vr->param.file.filp = filp;
    filp->refcnt++;
}
