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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/service.h>
#include "libmemfs/libmemfs.h"
#include "global.h"
#include "proto.h"

PUBLIC ssize_t procfs_read_hook(struct memfs_inode* inode, char* ptr, size_t count,
        off_t offset, cbdata_t data)
{
    init_buf(ptr, count, offset);

    if (memfs_node_index(inode) == NO_INDEX) {
        ((void (*)(void))data)();
    }

    return buf_used();
}
