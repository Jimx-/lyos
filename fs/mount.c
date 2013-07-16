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
#include "fcntl.h"

PUBLIC void clear_vfs_mount(struct vfs_mount * vmnt)
{
    vmnt->m_fs_ep = NO_TASK;
    vmnt->m_dev = NO_DEV;
    vmnt->m_flags = 0;
    vmnt->m_mounted_on = 0;
    vmnt->m_root_node = 0;
    vmnt->m_label[0] = '\0';
}

PUBLIC struct vfs_mount * get_free_vfs_mount()
{
    struct  vfs_mount * vmnt = vmnt_table;
    for (; vmnt < &vmnt_table[NR_VFS_MOUNT]; vmnt++)
    {
        if (vmnt->m_dev == NO_DEV) {
            clear_vfs_mount(vmnt);
            return vmnt;
        }
    }

    return (struct vfs_mount *)0;
}