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
#include <lyos/mgrant.h>
#include <sys/stat.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"

int request_lookup(endpoint_t fs_e, dev_t dev, ino_t start, ino_t root,
                   struct lookup* lookup, struct fproc* fp,
                   struct lookup_result* ret)
{
    MESSAGE m;
    mgrant_id_t path_grant;
    size_t name_len;
    int retval;

    name_len = strlen(lookup->pathname);
    path_grant = mgrant_set_direct(fs_e, (vir_bytes)lookup->pathname, name_len,
                                   MGF_READ | MGF_WRITE);
    if (path_grant == GRANT_INVALID)
        panic("vfs: request_lookup cannot create path grant");

    m.type = FS_LOOKUP;
    m.u.m_vfs_fs_lookup.dev = dev;
    m.u.m_vfs_fs_lookup.start = start;
    m.u.m_vfs_fs_lookup.root = root;
    m.u.m_vfs_fs_lookup.name_len = name_len;
    m.u.m_vfs_fs_lookup.path_grant = path_grant;
    m.u.m_vfs_fs_lookup.user_endpt = fp->endpoint;
    m.u.m_vfs_fs_lookup.flags = lookup->flags;

    m.u.m_vfs_fs_lookup.uid = fp->effuid;
    m.u.m_vfs_fs_lookup.gid = fp->effgid;

    memset(ret, 0, sizeof(struct lookup_result));
    fs_sendrec(fs_e, &m);

    mgrant_revoke(path_grant);

    retval = m.u.m_fs_vfs_lookup_reply.status;
    ret->fs_ep = m.source;

    switch (retval) {
    case 0:
        ret->inode_nr = m.u.m_fs_vfs_lookup_reply.num;
        ret->uid = m.u.m_fs_vfs_lookup_reply.node.uid;
        ret->gid = m.u.m_fs_vfs_lookup_reply.node.gid;
        ret->size = m.u.m_fs_vfs_lookup_reply.node.size;
        ret->mode = m.u.m_fs_vfs_lookup_reply.node.mode;
        ret->spec_dev = m.u.m_fs_vfs_lookup_reply.node.spec_dev;
        ret->dev = dev;
        break;
    case EENTERMOUNT:
        ret->offsetp = m.u.m_fs_vfs_lookup_reply.offset;
        ret->inode_nr = m.u.m_fs_vfs_lookup_reply.num;
        break;
    case ELEAVEMOUNT:
        ret->offsetp = m.u.m_fs_vfs_lookup_reply.offset;
        ret->dev = dev;
        break;
    case ESYMLINK:
        ret->offsetp = m.u.m_fs_vfs_lookup_reply.offset;
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
    lookup->dirfd = AT_FDCWD;
    lookup->vmnt_lock = RWL_NONE;
    lookup->inode_lock = RWL_NONE;
    lookup->vmnt = vmnt;
    lookup->inode = inode;
    *vmnt = NULL;
    *inode = NULL;
}

void init_lookupat(struct lookup* lookup, int dfd, char* pathname, int flags,
                   struct vfs_mount** vmnt, struct inode** inode)
{
    lookup->pathname = pathname;
    lookup->flags = flags;
    lookup->dirfd = dfd;
    lookup->vmnt_lock = RWL_NONE;
    lookup->inode_lock = RWL_NONE;
    lookup->vmnt = vmnt;
    lookup->inode = inode;
    *vmnt = NULL;
    *inode = NULL;
}

static struct inode* get_start_inode(struct lookup* lookup, struct fproc* fp)
{
    struct inode* start = NULL;
    struct file_desc* filp;

    /* Absolute pathname -- fetch the root. */
    if (lookup->pathname[0] == '/') {
        if (!fp->root) {
            printl("vfs: process %d has no root directory\n", fp->endpoint);
            err_code = ENOENT;
        }

        return fp->root;
    }

    /* Relative pathname -- get the starting-point it is relative to. */

    if (lookup->dirfd == AT_FDCWD) {
        if (!fp->pwd) {
            printl("vfs: process %d has no working directory\n", fp->endpoint);
            err_code = ENOENT;
        }

        return fp->pwd;
    }

    filp = get_filp(fp, lookup->dirfd, RWL_READ);
    if (!filp || !filp->fd_inode) {
        err_code = EBADF;
        return NULL;
    }

    if (!S_ISDIR(filp->fd_inode->i_mode)) {
        err_code = ENOTDIR;
        goto out;
    }

    start = filp->fd_inode;

out:
    unlock_filp(filp);
    return start;
}

struct inode* resolve_path(struct lookup* lookup, struct fproc* fp)
{
    struct inode* start;

    start = get_start_inode(lookup, fp);
    if (!start) return NULL;

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
        if (ret == EBUSY || ret == EDEADLK) {
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
                if (vmnt) unlock_vmnt(vmnt);
                *(lookup->vmnt) = NULL;
                err_code = EIO;
                return NULL;
            }
        } else if (ret == ELEAVEMOUNT) {
            /* Are we really leaving a filesystem? */
            if (memcmp(lookup->pathname, "..", 2) != 0 ||
                (lookup->pathname[2] != '\0' && lookup->pathname[2] != '/')) {
                if (vmnt) unlock_vmnt(vmnt);
                *(lookup->vmnt) = NULL;
                err_code = ENOENT;
                return NULL;
            }

            dir_pin = vmnt->m_mounted_on;
        } else {
            /* ESYMLINK */
            dir_pin = fproc->root;
            vmnt_tmp = NULL;
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
            if (ret == EBUSY || ret == EDEADLK) {
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

        if ((vmnt = find_vfs_mount(new_pin->i_dev)) == NULL)
            panic("VFS: resolve_path: vmnt not found");
        new_pin->i_vmnt = vmnt;

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
        struct inode* start = get_start_inode(lookup, fp);
        if (!start) return NULL;

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

int do_socketpath(void)
{
    endpoint_t endpoint = self->msg_in.u.m_vfs_socketpath.endpoint;
    mgrant_id_t grant = self->msg_in.u.m_vfs_socketpath.grant;
    size_t size = self->msg_in.u.m_vfs_socketpath.size;
    int request = self->msg_in.u.m_vfs_socketpath.request;
    char path[PATH_MAX + 1];
    struct fproc* fp;
    struct lookup lookup, lookup2;
    struct vfs_mount *vmnt, *vmnt2;
    struct inode *dir_pin, *pin;
    mode_t mode;
    int retval;

    if (fproc->realgid != SU_UID) return EPERM;

    if ((fp = vfs_endpt_proc(endpoint)) == NULL) return EINVAL;

    if (size == 0 || size > PATH_MAX) return EINVAL;
    if ((retval = safecopy_from(fproc->endpoint, grant, 0, path, size)) != OK)
        return retval;
    path[size] = '\0';

    switch (request) {
    case SKP_CREATE:
        init_lookup(&lookup, path, LKF_SYMLINK_NOFOLLOW, &vmnt, &dir_pin);
        lookup.vmnt_lock = RWL_WRITE;
        lookup.inode_lock = RWL_WRITE;

        if ((dir_pin = last_dir(&lookup, fp)) == NULL) return err_code;

        mode = S_IFSOCK | (ACCESSPERMS & fp->umask);

        if (!S_ISDIR(dir_pin->i_mode))
            retval = ENOTDIR;
        else if ((retval = forbidden(fp, dir_pin, W_BIT | X_BIT)) == 0) {
            retval =
                request_mknod(dir_pin->i_fs_ep, dir_pin->i_dev, dir_pin->i_num,
                              fp->effuid, fp->effgid, path, mode, NO_DEV);

            if (retval == 0) {
                init_lookup(&lookup2, path, LKF_SYMLINK_NOFOLLOW, &vmnt2, &pin);
                lookup2.vmnt_lock = RWL_READ;
                lookup2.inode_lock = RWL_READ;
                pin = advance_path(dir_pin, &lookup2, fp);

                if (pin) {
                    self->msg_out.u.m_vfs_socketpath.dev = pin->i_dev;
                    self->msg_out.u.m_vfs_socketpath.num = pin->i_num;

                    unlock_inode(pin);
                    put_inode(pin);
                } else {
                    retval = err_code;
                }
            } else if (retval == EEXIST)
                retval = EADDRINUSE;
        }

        unlock_inode(dir_pin);
        unlock_vmnt(vmnt);
        put_inode(dir_pin);
        break;
    case SKP_LOOK_UP:
        init_lookup(&lookup, path, 0, &vmnt, &pin);
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_READ;

        if ((pin = resolve_path(&lookup, fp)) == NULL) return err_code;

        if (!S_ISSOCK(pin->i_mode))
            retval = ENOTSOCK;
        else
            retval = forbidden(fp, pin, R_BIT | W_BIT);

        if (retval == OK) {
            self->msg_out.u.m_vfs_socketpath.dev = pin->i_dev;
            self->msg_out.u.m_vfs_socketpath.num = pin->i_num;
        }

        unlock_inode(pin);
        unlock_vmnt(vmnt);
        put_inode(pin);
        break;
    default:
        return EINVAL;
    }

    return retval;
}
