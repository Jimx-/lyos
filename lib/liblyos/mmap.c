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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <sys/mman.h>
#include <string.h>

void* mmap_for(endpoint_t forwhom, void* addr, size_t len, int prot, int flags,
               int fd, off_t offset)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = MMAP;
    m.u.m_mm_mmap.who = forwhom;
    m.u.m_mm_mmap.vaddr = addr;
    m.u.m_mm_mmap.length = len;
    m.u.m_mm_mmap.prot = prot;
    m.u.m_mm_mmap.flags = flags;
    m.u.m_mm_mmap.fd = fd;
    m.u.m_mm_mmap.offset = offset;

    send_recv(BOTH, TASK_MM, &m);

    if (m.u.m_mm_mmap_reply.retval) return MAP_FAILED;
    return m.u.m_mm_mmap_reply.retaddr;
}

int vfs_mmap(endpoint_t who, off_t offset, size_t len, dev_t dev, ino_t ino,
             int fd, void* vaddr, int flags, int prot, size_t clearend)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = FS_MMAP;
    m.u.m_mm_mmap.who = who;
    m.u.m_mm_mmap.offset = offset;
    m.u.m_mm_mmap.length = len;
    m.u.m_mm_mmap.dev = dev;
    m.u.m_mm_mmap.ino = ino;
    m.u.m_mm_mmap.fd = fd;
    m.u.m_mm_mmap.vaddr = vaddr;
    m.u.m_mm_mmap.flags = flags;
    m.u.m_mm_mmap.prot = prot;
    m.u.m_mm_mmap.clearend = clearend;

    send_recv(BOTH, TASK_MM, &m);

    return m.u.m_mm_mmap_reply.retval;
}

void* mm_remap(endpoint_t dest, endpoint_t src, void* dest_addr, void* src_addr,
               size_t size)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = MM_REMAP;
    m.u.m_mm_remap.src = src;
    m.u.m_mm_remap.dest = dest;
    m.u.m_mm_remap.src_addr = src_addr;
    m.u.m_mm_remap.dest_addr = dest_addr;
    m.u.m_mm_remap.size = size;

    send_recv(BOTH, TASK_MM, &m);

    return m.u.m_mm_remap.ret_addr;
}
