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
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "errno.h"
#include "fcntl.h"
#include "path.h"
#include "global.h"
#include "proto.h"

PUBLIC void init_inode_table()
{
    int i;
    for (i = 0; i < INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&vfs_inode_table[i]);
    }
}

PRIVATE void addhash_inode(struct inode * pin)
{
    unsigned int hash = pin->i_num & INODE_HASH_MASK;

    list_add(&(pin->list), &vfs_inode_table[hash]);
}

PRIVATE void unhash_inode(struct inode * pin)
{
    list_del(&(pin->list));
}

PUBLIC void clear_inode(struct inode * pin)
{
    pin->i_cnt = 0;
    pin->i_dev = 0;
    pin->i_num = 0;
    spinlock_init(&(pin->i_lock));
}

PUBLIC struct inode * new_inode(dev_t dev, ino_t num)
{
    struct inode * pin = (struct inode *)sbrk(sizeof(struct inode));
    if (!pin) return NULL;

    clear_inode(pin);
    
    pin->i_dev = dev;
    pin->i_num = num;

    addhash_inode(pin);
    return pin;
}

PUBLIC struct inode * find_inode(dev_t dev, ino_t num)
{
    int hash = num & INODE_HASH_MASK;

    struct inode * pin;
    list_for_each_entry(pin, &vfs_inode_table[hash], list) {
        if ((pin->i_num == num) && (pin->i_dev == dev)) {   /* hit */
            return pin;
        }
    }
    return NULL;
}

PUBLIC void put_inode(struct inode * pin)
{
    if (!pin) return;

    lock_inode(pin);

    if (pin->i_cnt > 1) {
        pin->i_cnt--;
        unlock_inode(pin);
        return;
    }

    if (pin->i_cnt <= 0) panic("VFS: put_inode: pin->i_cnt is already <= 0");
    
    int ret;
    if ((ret = request_put_inode(pin->i_fs_ep, pin->i_dev, pin->i_num)) != 0) {
        err_code = ret;
        unlock_inode(pin);
        return;
    }

    unhash_inode(pin);
}

PUBLIC void lock_inode(struct inode * pin)
{
    spinlock_lock(&(pin->i_lock));
}

PUBLIC void unlock_inode(struct inode * pin)
{
    spinlock_unlock(&(pin->i_lock));
}
        
PUBLIC int request_put_inode(endpoint_t fs_e, dev_t dev, ino_t num)
{
    MESSAGE m;
    m.type = FS_PUTINODE;
    m.REQ_DEV = dev;
    m.REQ_NUM = num;

    send_recv(BOTH, fs_e, &m);
    return m.RET_RETVAL;
}
