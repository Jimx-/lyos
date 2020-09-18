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
#include <sys/syslimits.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"

int request_lookup(endpoint_t fs_e, dev_t dev, ino_t start, ino_t root,
                   struct lookup* lookup, struct fproc* fp,
                   struct lookup_result* ret)
{
    MESSAGE m;
    int retval;

    struct vfs_ucred ucred;
    ucred.uid = fp->effuid;
    ucred.gid = fp->effgid;

    m.type = FS_LOOKUP;
    m.REQ_DEV = dev;
    m.REQ_START_INO = start;
    m.REQ_ROOT_INO = root;
    m.REQ_NAMELEN = strlen(lookup->pathname);
    m.REQ_PATHNAME = lookup->pathname;
    m.REQ_FLAGS = lookup->flags;
    m.REQ_UCRED = &ucred;

    memset(ret, 0, sizeof(struct lookup_result));
    fs_sendrec(fs_e, &m);

    retval = m.RET_RETVAL;
    ret->fs_ep = m.source;

    switch (retval) {
    case 0:
        ret->inode_nr = m.RET_NUM;
        ret->uid = m.RET_UID;
        ret->gid = m.RET_GID;
        ret->size = m.RET_FILESIZE;
        ret->mode = m.RET_MODE;
        ret->spec_dev = m.RET_SPECDEV;
        ret->dev = dev;
        break;
    case EENTERMOUNT:
        ret->offsetp = m.RET_OFFSET;
        ret->inode_nr = m.RET_NUM;
        break;
    case ELEAVEMOUNT:
        ret->offsetp = m.RET_OFFSET;
        break;
    default:
        break;
    }

    return retval;
}

void init_lookup(struct lookup* lookup, char* pathname, int flags,
                 struct vfs_mount** vmnt, struct inode** inode)
{
    lookup->pathname = pathname;
    lookup->flags = flags;
    lookup->vmnt_lock = RWL_NONE;
    lookup->inode_lock = RWL_NONE;
    lookup->vmnt = vmnt;
    lookup->inode = inode;
    *vmnt = NULL;
    *inode = NULL;
}

struct inode* resolve_path(struct lookup* lookup, struct fproc* fp)
{
    /* start inode: root or pwd */
    if (!fp->root || !fp->pwd) {
        printl(
            "VFS: resolve_path: process #%d has no root or working directory\n",
            fp->endpoint);
        err_code = ENOENT;
        return NULL;
    }

    struct inode* start = (lookup->pathname[0] == '/' ? fp->root : fp->pwd);
    return advance_path(start, lookup, fp);
}

/* Resolve a pathname */
struct inode* advance_path(struct inode* start, struct lookup* lookup,
                           struct fproc* fp)
{
    char* pathname = lookup->pathname;
    struct inode *new_pin = NULL, *pin = NULL;

    struct lookup_result res;

    /* empty pathname */
    if (pathname[0] == '\0') {
        err_code = ENOENT;
        return NULL;
    }

    int dev = start->i_dev;
    int num = start->i_num;

    ino_t root_ino = 0;
    struct vfs_mount *vmnt_tmp, *vmnt = find_vfs_mount(dev);
    if (vmnt == NULL) {
        err_code = EIO; /* no such mountpoint */
        return NULL;
    }

    int ret = lock_vmnt(vmnt, RWL_WRITE);
    if (ret) {
        if (ret == EBUSY) {
            vmnt = NULL;
        } else {
            return NULL;
        }
    }
    *lookup->vmnt = vmnt;

    if (fp->root->i_dev == fp->pwd->i_dev) root_ino = fp->root->i_num;

    ret = request_lookup(start->i_fs_ep, dev, num, root_ino, lookup, fp, &res);
    if (ret != 0 && ret != EENTERMOUNT && ret != ELEAVEMOUNT) {
        if (vmnt) unlock_vmnt(vmnt);
        *(lookup->vmnt) = NULL;
        err_code = ret;
        return NULL;
    }

    /* TODO: deal with EENTERMOUNT and ELEAVEMOUNT */
    while (ret == EENTERMOUNT || ret == ELEAVEMOUNT) {
        int path_offset = res.offsetp;
        int left_len = strlen(&pathname[path_offset]);
        memmove(pathname, &pathname[path_offset], left_len);
        pathname[left_len] = '\0';

        struct inode* dir_pin = NULL;
        if (ret == EENTERMOUNT) {
            list_for_each_entry(vmnt_tmp, &vfs_mount_table, list)
            {
                if (vmnt_tmp->m_dev != 0 && vmnt_tmp->m_mounted_on != NULL) {
                    if (vmnt_tmp->m_mounted_on->i_num == res.inode_nr &&
                        vmnt_tmp->m_mounted_on->i_fs_ep == res.fs_ep) {
                        dir_pin = vmnt_tmp->m_root_node;
                        break;
                    }
                }
            }

            if (dir_pin == NULL) {
                if (vmnt_tmp) unlock_vmnt(vmnt_tmp);
                *(lookup->vmnt) = NULL;
                err_code = EIO;
                return NULL;
            }
        } else if (ret == ELEAVEMOUNT) {
        }

        endpoint_t fs_e = dir_pin->i_fs_ep;
        ino_t dir_num = dir_pin->i_num;

        if (dir_pin->i_dev == fp->root->i_dev)
            root_ino = fp->root->i_num;
        else
            root_ino = 0;

        if (vmnt) unlock_vmnt(vmnt);
        vmnt = find_vfs_mount(dir_pin->i_dev);
        if (!vmnt) {
            *(lookup->vmnt) = NULL;
            err_code = EIO;
            return NULL;
        }

        ret = lock_vmnt(vmnt, RWL_WRITE);
        if (ret) {
            if (ret == EBUSY) {
                vmnt = NULL;
            } else {
                return NULL;
            }
        }
        *lookup->vmnt = vmnt;

        ret = request_lookup(fs_e, dir_pin->i_dev, dir_num, root_ino, lookup,
                             fp, &res);

        if (ret != 0 && ret != EENTERMOUNT && ret != ELEAVEMOUNT) {
            if (vmnt) unlock_vmnt(vmnt);
            *(lookup->vmnt) = NULL;
            err_code = ret;
            return NULL;
        }
    }

    if ((*lookup->vmnt) != NULL && lookup->vmnt_lock != RWL_WRITE) {
        /* downgrade vmnt to requested lock level */
        unlock_vmnt(*lookup->vmnt);
        lock_vmnt(*lookup->vmnt, RWL_READ);
    }

    if ((pin = find_inode(res.dev, res.inode_nr)) == NULL) { /* not found */
        new_pin = new_inode(res.dev, res.inode_nr, res.mode);

        if (new_pin == NULL) return NULL;

        lock_inode(new_pin, lookup->inode_lock);
        new_pin->i_dev = res.dev;
        new_pin->i_num = res.inode_nr;
        new_pin->i_gid = res.gid;
        new_pin->i_uid = res.uid;
        new_pin->i_size = res.size;
        new_pin->i_fs_ep = res.fs_ep;
        new_pin->i_specdev = res.spec_dev;
        new_pin->i_vmnt = vmnt;
        if (!new_pin->i_vmnt) panic("VFS: resolve_path: vmnt not found");

        pin = new_pin;
    } else {
        lock_inode(pin, lookup->inode_lock);
    }

    *(lookup->inode) = pin;
    pin->i_cnt++;

    return pin;
}

/**
 * Parse the path down to the last directory.
 * @param  pathname The path to be parsed.
 * @param  fp       The caller.
 * @return          Inode of the last directory on success. Otherwise NULL.
 */
struct inode* last_dir(struct lookup* lookup, struct fproc* fp)
{
    struct inode* result_pin = NULL;
    char* cp;
    char entry[PATH_MAX + 1];

    do {
        struct inode* start = (lookup->pathname[0] == '/' ? fp->root : fp->pwd);

        size_t len = strlen(lookup->pathname);
        if (len == 0) {
            errno = ENOENT;
            result_pin = NULL;
            break;
        }

        while (len > 1 && lookup->pathname[len - 1] == '/') {
            len--;
            lookup->pathname[len] = '\0';
        }

        cp = strrchr(lookup->pathname, '/');
        if (cp == NULL) {
            if (strlcpy(entry, lookup->pathname, PATH_MAX + 1) >=
                PATH_MAX + 1) {
                errno = ENAMETOOLONG;
                result_pin = NULL;
                break;
            }
            entry[PATH_MAX] = '\0';
            lookup->pathname[0] = '.';
            lookup->pathname[1] = '\0';
        } else if (cp[1] == '/') {
            entry[0] = '.';
            entry[1] = '\0';
        } else {
            if (strlcpy(entry, cp + 1, PATH_MAX + 1) >= PATH_MAX + 1) {
                errno = ENAMETOOLONG;
                result_pin = NULL;
                break;
            }
            cp[1] = '\0';
            entry[PATH_MAX] = '\0';
        }

        while (cp > lookup->pathname && *cp == '/') {
            *cp = '\0';
            cp--;
        }

        if ((result_pin = advance_path(start, lookup, fp)) == NULL) {
            break;
        }

        strlcpy(lookup->pathname, entry, PATH_MAX + 1);
        break;
    } while (0);

    return result_pin;
}
