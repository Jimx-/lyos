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

#include <lyos/ipc.h>
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "region.h"
#include "proto.h"
#include "global.h"

static DEF_LIST(file_list);

static struct mm_file_desc* alloc_mm_file_desc(int fd, dev_t dev, ino_t ino)
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

struct mm_file_desc* get_mm_file_desc(int fd, dev_t dev, ino_t ino)
{
    struct mm_file_desc* filp;
    list_for_each_entry(filp, &file_list, list)
    {
        if (filp->dev == dev && filp->ino == ino && filp->fd == fd) return filp;
    }

    return alloc_mm_file_desc(fd, dev, ino);
}

void file_reference(struct vir_region* vr, struct mm_file_desc* filp)
{
    vr->param.file.filp = filp;
    filp->refcnt++;
}

void file_unreferenced(struct mm_file_desc* filp)
{
    filp->refcnt--;
    if (filp->refcnt <= 0) {
        enqueue_vfs_request(&mmproc_table[TASK_MM], MMR_FDCLOSE, filp->fd, 0, 0,
                            0, NULL, NULL, 0);
        list_del(&filp->list);
        SLABFREE(filp);
    }
}
