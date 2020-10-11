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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"

static struct vfs_mount* anon_vmnt;
static struct inode* anon_inode;

void mount_anonfs(void)
{
    dev_t dev;
    struct inode* pin;

    if ((dev = get_none_dev()) == NO_DEV) {
        panic("vfs: cannot allocate dev for anonfs");
    }

    if ((anon_vmnt = get_free_vfs_mount()) == NULL) {
        panic("vfs: cannot allocate vfs mount for anonfs");
    }

    anon_vmnt->m_dev = dev;
    anon_vmnt->m_fs_ep = NO_TASK;
    anon_vmnt->m_flags = 0;
    strlcpy(anon_vmnt->m_label, "anonfs", FS_LABEL_MAX);

    anon_vmnt->m_mounted_on = NULL;
    anon_vmnt->m_root_node = NULL;

    pin = new_inode(anon_vmnt->m_dev, 1, S_IRUSR | S_IWUSR);

    if (!pin) {
        panic("vfs: cannot allocate inode for anonfs");
    }

    pin->i_dev = anon_vmnt->m_dev;
    pin->i_num = 1;
    pin->i_gid = 0;
    pin->i_uid = 0;
    pin->i_size = 0;
    pin->i_fs_ep = NO_TASK;
    pin->i_specdev = NO_DEV;
    pin->i_vmnt = anon_vmnt;
    pin->i_fops = NULL;
    pin->i_private = NULL;

    pin->i_cnt++;

    anon_inode = pin;
}

int anon_inode_get_fd(struct fproc* fproc, int start,
                      const struct file_operations* fops, void* private,
                      int flags)
{
    static char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};

    int retval;
    int fd;
    mode_t bits = mode_map[flags & O_ACCMODE];
    struct file_desc* filp;

    retval = lock_vmnt(anon_vmnt, RWL_READ);
    if (retval) return -retval;

    retval = lock_inode(anon_inode, RWL_READ);
    if (retval) {
        unlock_vmnt(anon_vmnt);
        return -retval;
    }

    if ((retval = get_fd(fproc, 0, bits, &fd, &filp)) != 0) {
        unlock_inode(anon_inode);
        unlock_vmnt(anon_vmnt);
        return -retval;
    }

    fproc->filp[fd] = filp;
    filp->fd_cnt = 1;

    filp->fd_inode = anon_inode;
    anon_inode->i_cnt++;
    filp->fd_flags = flags & (O_ACCMODE | O_NONBLOCK);

    filp->fd_fops = fops;
    filp->fd_private_data = private;

    unlock_filp(filp);
    unlock_vmnt(anon_vmnt);

    return fd;
}
