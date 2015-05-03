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
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include <libsysfs/libsysfs.h>
#include "node.h"
#include "proto.h"

PUBLIC ssize_t sysfs_read_hook(struct memfs_inode* inode, char* ptr, size_t count,
        off_t offset, cbdata_t data)
{
    init_buf(ptr, count, offset);

    sysfs_node_t* node = (sysfs_node_t*)data;

    switch (NODE_TYPE(node)) {
    case SF_TYPE_U32:
        buf_printf("%d", node->u.u32v);
    }

    buf_printf("\n");
    
    return buf_used();
}
