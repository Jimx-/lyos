/**
 * @file mount.c
 * @brief VFS mount functionality
 * @author jimx, develorcer@gmail.com
 * @version 0.3.2
 * @date 2013-07-28
 */
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
#define __LINUX_ERRNO_EXTENSIONS__ /* we want ENOTBLK */
#include "errno.h"
#include "fcntl.h"
#include "lyos/list.h"
#include "const.h"
#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"

/* find device number of the given pathname */
static dev_t name2dev(struct fproc* fp, char* pathname);
// static int is_none_dev(dev_t dev);
static int request_mountpoint(endpoint_t fs_ep, dev_t dev, ino_t num);

void clear_vfs_mount(struct vfs_mount* vmnt)
{
    vmnt->m_fs_ep = NO_TASK;
    vmnt->m_dev = NO_DEV;
    vmnt->m_flags = 0;
    vmnt->m_mounted_on = 0;
    vmnt->m_root_node = 0;
    vmnt->m_label[0] = '\0';
    rwlock_init(&(vmnt->m_lock));
}

struct vfs_mount* get_free_vfs_mount()
{
    struct vfs_mount* vmnt =
        (struct vfs_mount*)malloc(sizeof(struct vfs_mount));
    if (vmnt == NULL) {
        err_code = ENOMEM;
        return NULL;
    }
    clear_vfs_mount(vmnt);
    list_add(&(vmnt->list), &vfs_mount_table);

    return vmnt;
}

struct vfs_mount* find_vfs_mount(dev_t dev)
{
    struct vfs_mount* vmnt;
    list_for_each_entry(vmnt, &vfs_mount_table, list)
    {
        if (vmnt->m_dev == dev) {
            return vmnt;
        }
    }

    return NULL;
}

/**
 * Lock the vfs mount.
 * @param vmnt
 */
int lock_vmnt(struct vfs_mount* vmnt, rwlock_type_t type)
{
    return rwlock_lock(&(vmnt->m_lock), type);
}

/**
 * Unlock the vfs mount.
 * @param vmnt
 */
void unlock_vmnt(struct vfs_mount* vmnt) { rwlock_unlock(&(vmnt->m_lock)); }

int do_mount(void)
{
    unsigned long flags = self->msg_in.MFLAGS;

    /* find fs endpoint */
    int label_len = self->msg_in.MNAMELEN3;
    char fs_label[FS_LABEL_MAX];
    if (label_len > FS_LABEL_MAX) return ENAMETOOLONG;

    data_copy(SELF, fs_label, fproc->endpoint, self->msg_in.MLABEL, label_len);
    fs_label[label_len] = '\0';

    int fs_e = get_filesystem_endpoint(fs_label);

    if (fs_e == -1) return EINVAL;

    int source_len = self->msg_in.MNAMELEN1;
    int target_len = self->msg_in.MNAMELEN2;

    char source[MAX_PATH];
    if (source_len > MAX_PATH) return ENAMETOOLONG;

    char target[MAX_PATH];
    if (target_len > MAX_PATH) return ENAMETOOLONG;

    data_copy(SELF, target, fproc->endpoint, self->msg_in.MTARGET, target_len);
    target[target_len] = '\0';

    int is_root = strcmp(target, "/") == 0;
    int mount_root = (is_root && have_root < 2); /* root can be mounted twice */

    dev_t dev_nr;
    if (mount_root) {
        /* /dev is not available before we mount root so we need to parse it
         * differently */
        data_copy(SELF, source, fproc->endpoint, self->msg_in.MSOURCE,
                  source_len);
        source[source_len] = '\0';

        int major, minor;
        char dummy;
        if (sscanf(source, "dev(%u,%u)%c", &major, &minor, &dummy) == 2) {
            dev_nr = MAKE_DEV(major, minor);
        } else {
            return EINVAL;
        }
    } else if (source_len > 0) {
        data_copy(SELF, source, fproc->endpoint, self->msg_in.MSOURCE,
                  source_len);
        source[source_len] = '\0';

        dev_nr = name2dev(fproc, source);
        if (dev_nr == 0) return err_code;
    } else {
        dev_nr = get_none_dev();
        if (dev_nr == 0) return err_code;
    }

    int readonly = flags & MS_READONLY;
    int retval = mount_fs(dev_nr, target, fs_e, readonly);

    if (retval == 0 && is_root)
        printl("VFS: %s is %s mounted on %s\n", source,
               readonly ? "read-only" : "read-write", target);
    return retval;
}

int mount_fs(dev_t dev, char* mountpoint, endpoint_t fs_ep, int readonly)
{
    if (find_vfs_mount(dev) != NULL) return EBUSY;
    struct vfs_mount* new_pvm = get_free_vfs_mount();
    if (new_pvm == NULL) return ENOMEM;

    lock_vmnt(new_pvm, RWL_WRITE);

    int retval = 0;
    struct inode* pmp = NULL;
    struct vfs_mount* parent_vmnt = NULL;

    int is_root = (strcmp(mountpoint, "/") == 0);
    int mount_root = (is_root && have_root < 2); /* root can be mounted twice */

    if (!mount_root) {
        /* resolve the mountpoint */
        struct lookup lookup;
        init_lookup(&lookup, mountpoint, 0, &parent_vmnt, &pmp);
        lookup.vmnt_lock = RWL_WRITE;
        lookup.inode_lock = RWL_WRITE;
        pmp = resolve_path(&lookup, fproc);

        if (pmp == NULL)
            retval = err_code;
        else if (pmp->i_cnt == 1)
            retval = request_mountpoint(pmp->i_fs_ep, pmp->i_dev, pmp->i_num);
        else
            retval = EBUSY;

        if (pmp) {
            unlock_vmnt(parent_vmnt);
        }

        if (retval) {
            if (pmp) {
                unlock_inode(pmp);
                put_inode(pmp);
            }
            unlock_vmnt(new_pvm);
            return retval;
        }
    }

    new_pvm->m_dev = dev;
    new_pvm->m_fs_ep = fs_ep;

    if (readonly)
        new_pvm->m_flags |= VMNT_READONLY;
    else
        new_pvm->m_flags &= ~VMNT_READONLY;

    struct lookup_result res;
    retval = request_readsuper(fs_ep, dev, readonly, is_root, &res);

    if (retval) {
        if (pmp) {
            unlock_inode(pmp);
            put_inode(pmp);
        }
        unlock_vmnt(new_pvm);
        return retval;
    }

    struct inode* root_inode = new_inode(dev, res.inode_nr, res.mode);

    if (!root_inode) {
        if (pmp) {
            unlock_inode(pmp);
            put_inode(pmp);
        }
        unlock_vmnt(new_pvm);
        return err_code;
    }

    lock_inode(root_inode, RWL_WRITE);
    root_inode->i_fs_ep = fs_ep;
    root_inode->i_gid = res.gid;
    root_inode->i_uid = res.uid;
    root_inode->i_size = res.size;
    root_inode->i_specdev = 0;
    root_inode->i_cnt = 1;
    root_inode->i_vmnt = new_pvm;

    if (mount_root) {
        new_pvm->m_root_node = root_inode;
        new_pvm->m_mounted_on = NULL;
        ROOT_DEV = dev;

        /* update all root inodes */
        struct fproc* fp = fproc_table;
        int i;
        for (i = 0; i < NR_PROCS; i++, fp++) {

            if (fp->root) put_inode(fp->root);
            root_inode->i_cnt++;
            fp->root = root_inode;

            if (fp->pwd) put_inode(fp->pwd);
            root_inode->i_cnt++;
            fp->pwd = root_inode;
        }

        have_root++;
        unlock_vmnt(new_pvm);
        unlock_inode(root_inode);
        return 0;
    }

    new_pvm->m_mounted_on = pmp;
    new_pvm->m_root_node = root_inode;

    unlock_inode(pmp);
    unlock_vmnt(new_pvm);
    unlock_inode(root_inode);

    return 0;
}

/**
 * @brief name2dev Find the device number of the given pathname.
 *
 * @param pathname Path to the block/character special file.
 *
 * @return The device number of the given pathname.
 */
static dev_t name2dev(struct fproc* fp, char* pathname)
{
    struct vfs_mount* vmnt = NULL;
    struct inode* pin = NULL;
    struct lookup lookup;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.inode_lock = RWL_READ;
    lookup.vmnt_lock = RWL_READ;
    pin = resolve_path(&lookup, fp);

    if (pin == NULL) {
        return (dev_t)0;
    }

    dev_t retval = 0;
    if (S_ISBLK(pin->i_mode))
        retval = pin->i_specdev;
    else
        err_code = ENOTBLK;

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);
    return retval;
}

/*
static int is_none_dev(dev_t dev)
{
    return (MAJOR(dev) == MAJOR_NONE) && (MINOR(dev) < NR_NONEDEVS);
}
*/

dev_t get_none_dev(void)
{
    static int none_dev = 1;

    if (none_dev < NR_NONEDEVS) return MAKE_DEV(MAJOR_NONE, none_dev++);

    return 0;
}

static int request_mountpoint(endpoint_t fs_ep, dev_t dev, ino_t num)
{
    MESSAGE m;
    m.type = FS_MOUNTPOINT;
    m.REQ_DEV = dev;
    m.REQ_NUM = num;

    fs_sendrec(fs_ep, &m);

    return m.RET_RETVAL;
}
