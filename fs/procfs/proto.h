/*
    This file is part of Lyos.

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

#ifndef _PROCFS_PROTO_H_
#define _PROCFS_PROTO_H_

#include <libmemfs/libmemfs.h>

ssize_t procfs_read_hook(struct memfs_inode* inode, char* ptr, size_t count,
                         off_t offset, cbdata_t data);
int procfs_lookup_hook(struct memfs_inode* parent, const char* name,
                       cbdata_t data);
int procfs_getdents_hook(struct memfs_inode* inode, cbdata_t data);
int procfs_rdlink_hook(struct memfs_inode* inode, cbdata_t data,
                       struct memfs_inode** target, endpoint_t user_endpt);

void init_buf(char* ptr, size_t len, off_t off);
void buf_printf(char* fmt, ...);
size_t buf_used();

#endif
