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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <sys/mman.h>
#include <lyos/ipc.h>
#include <string.h>

PUBLIC void * mmap_for(endpoint_t forwhom,
    void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = MMAP;
    m.MMAP_WHO = forwhom;
    m.MMAP_VADDR = (int)addr;
    m.MMAP_LEN = len;
    m.MMAP_PROT = prot;
    m.MMAP_FLAGS = flags;
    m.MMAP_FD = fd;
    m.MMAP_OFFSET = offset;

    send_recv(BOTH, TASK_MM, &m);

    if (m.RETVAL) return MAP_FAILED;
    else return (void *)m.MMAP_RETADDR;
}

PUBLIC int vfs_mmap(endpoint_t who, off_t offset, size_t len,
    dev_t dev, ino_t ino, int fd, int vaddr, int flags)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = FS_MMAP;
    m.MMAP_WHO = who;
    m.MMAP_OFFSET = offset;
    m.MMAP_LEN = len;
    m.MMAP_DEV = dev;
    m.MMAP_INO = ino;
    m.MMAP_FD = fd;
    m.MMAP_VADDR = vaddr;
    m.MMAP_FLAGS = flags;

    send_recv(BOTH, TASK_MM, &m);

    return m.RETVAL;
}
