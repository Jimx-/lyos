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
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include "fcntl.h"
#include "const.h"
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"

static void set_inode_fops(struct inode* pin)
{
    if (S_ISCHR(pin->i_mode)) {
        pin->i_fops = &cdev_fops;
    } else if (S_ISREG(pin->i_mode)) {
        pin->i_fops = &vfs_fops;
    } else {
        pin->i_fops = NULL;
    }
}

void init_inode_table()
{
    int i;
    for (i = 0; i < INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&vfs_inode_table[i]);
    }
}

static void addhash_inode(struct inode* pin)
{
    unsigned int hash;

    assert(pin);

    hash = pin->i_num & INODE_HASH_MASK;

    list_add(&(pin->list), &vfs_inode_table[hash]);
}

static void unhash_inode(struct inode* pin) { list_del(&(pin->list)); }

void clear_inode(struct inode* pin)
{
    pin->i_cnt = 0;
    pin->i_fs_cnt = 0;
    pin->i_dev = 0;
    pin->i_num = 0;
    rwlock_init(&(pin->i_lock));
}

struct inode* new_inode(dev_t dev, ino_t num, mode_t mode)
{
    struct inode* pin = (struct inode*)malloc(sizeof(struct inode));
    if (!pin) return NULL;

    memset(pin, 0, sizeof(*pin));
    clear_inode(pin);

    pin->i_dev = dev;
    pin->i_num = num;
    pin->i_mode = mode;
    pin->i_fs_ep = NO_TASK;
    pin->i_private = NULL;

    set_inode_fops(pin);

    addhash_inode(pin);
    return pin;
}

struct inode* find_inode(dev_t dev, ino_t num)
{
    int hash = num & INODE_HASH_MASK;

    struct inode* pin;
    list_for_each_entry(pin, &vfs_inode_table[hash], list)
    {
        if ((pin->i_num == num) && (pin->i_dev == dev)) { /* hit */
            return pin;
        }
    }
    return NULL;
}

static int request_put_inode(endpoint_t fs_e, dev_t dev, ino_t num, int count)
{
    MESSAGE m;

    memset(&m, 0, sizeof(m));
    m.type = FS_PUTINODE;
    m.u.m_vfs_fs_putinode.dev = dev;
    m.u.m_vfs_fs_putinode.num = num;
    m.u.m_vfs_fs_putinode.count = count;

    fs_sendrec(fs_e, &m);

    return m.RETVAL;
}

void put_inode(struct inode* pin)
{
    int ret;

    if (!pin) return;

    lock_inode(pin, RWL_WRITE);

    assert(pin->i_cnt > 0);
    assert(pin->i_fs_cnt > 0);

    if (pin->i_cnt > 1) {
        pin->i_cnt--;
        unlock_inode(pin);
        return;
    }

    // TODO: handle this with vmnt operation
    if (pin->i_fs_ep != NO_TASK &&
        (ret = request_put_inode(pin->i_fs_ep, pin->i_dev, pin->i_num,
                                 pin->i_fs_cnt)) != 0) {
        err_code = ret;
        unlock_inode(pin);
        return;
    }

    unhash_inode(pin);
    unlock_inode(pin);

    free(pin);
}

struct inode* dup_inode(struct inode* pin)
{
    pin->i_cnt++;
    return pin;
}

int lock_inode(struct inode* pin, rwlock_type_t type)
{
    return rwlock_lock(&(pin->i_lock), type);
}

void unlock_inode(struct inode* pin) { rwlock_unlock(&(pin->i_lock)); }
