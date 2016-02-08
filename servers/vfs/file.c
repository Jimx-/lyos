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
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

PUBLIC void lock_filp(struct file_desc* filp)
{
    spinlock_lock(&filp->fd_lock);
}

PUBLIC void unlock_filp(struct file_desc* filp)
{
    spinlock_unlock(&filp->fd_lock);
}

PUBLIC struct file_desc* alloc_filp()
{
    int i;

    spinlock_lock(&f_desc_table_lock);
    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++) {
        struct file_desc* filp = &f_desc_table[i];

        if (f_desc_table[i].fd_inode == 0) {
            lock_filp(filp);
            spinlock_unlock(&f_desc_table_lock);
            return filp;
        }
    }
    spinlock_unlock(&f_desc_table_lock);

    return NULL;
}

PUBLIC int get_fd(struct fproc* fp)
{
    int i;

    for (i = 0; i < NR_FILES; i++) {
        if (fp->filp[i] == 0) {
            return i;
        }
    }
    
    return -ENFILE;
}
