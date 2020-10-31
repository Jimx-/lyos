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
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <lyos/mgrant.h>

#include "global.h"

static int request_ftrunc(endpoint_t fs_ep, dev_t dev, ino_t num, int newsize);

static int request_unlink(endpoint_t fs_ep, dev_t dev, ino_t num,
                          const char* last_component)
{
    MESSAGE m;
    size_t name_len;
    mgrant_id_t grant;

    name_len = strlen(last_component);
    grant =
        mgrant_set_direct(fs_ep, (vir_bytes)last_component, name_len, MGF_READ);
    if (grant == GRANT_INVALID)
        panic("vfs: request_unlink cannot create grant");

    m.type = FS_UNLINK;
    m.u.m_vfs_fs_unlink.dev = dev;
    m.u.m_vfs_fs_unlink.num = num;
    m.u.m_vfs_fs_unlink.grant = grant;
    m.u.m_vfs_fs_unlink.name_len = name_len;

    fs_sendrec(fs_ep, &m);
    mgrant_revoke(grant);

    return m.RETVAL;
}

static int request_rmdir(endpoint_t fs_ep, dev_t dev, ino_t num,
                         const char* last_component)
{
    MESSAGE m;
    size_t name_len;
    mgrant_id_t grant;

    name_len = strlen(last_component);
    grant =
        mgrant_set_direct(fs_ep, (vir_bytes)last_component, name_len, MGF_READ);
    if (grant == GRANT_INVALID)
        panic("vfs: request_unlink cannot create grant");

    m.type = FS_RMDIR;
    m.u.m_vfs_fs_unlink.dev = dev;
    m.u.m_vfs_fs_unlink.num = num;
    m.u.m_vfs_fs_unlink.grant = grant;
    m.u.m_vfs_fs_unlink.name_len = name_len;

    fs_sendrec(fs_ep, &m);
    mgrant_revoke(grant);

    return m.RETVAL;
}

static ssize_t request_rdlink(endpoint_t fs_ep, dev_t dev, ino_t num,
                              endpoint_t src, void* buf, size_t nbytes)
{
    MESSAGE m;
    mgrant_id_t grant;
    ssize_t retval;

    grant = mgrant_set_proxy(fs_ep, src, (vir_bytes)buf, nbytes, MGF_WRITE);
    if (grant == GRANT_INVALID)
        panic("vfs: request_rdlink failed to create proxy grant");

    m.type = FS_RDLINK;
    m.u.m_vfs_fs_readwrite.dev = dev;
    m.u.m_vfs_fs_readwrite.num = num;
    m.u.m_vfs_fs_readwrite.grant = grant;
    m.u.m_vfs_fs_readwrite.count = nbytes;
    m.u.m_vfs_fs_readwrite.user_endpt = src;

    fs_sendrec(fs_ep, &m);
    retval = m.u.m_vfs_fs_readwrite.status;

    mgrant_revoke(grant);

    if (retval == 0) {
        return m.u.m_vfs_fs_readwrite.count;
    }

    return -retval;
}

static int common_unlink(int dirfd, char* pathname, int flags)
{
    struct lookup lookup;
    struct vfs_mount *vmnt, *vmnt_pin;
    struct inode *pin_dir, *pin;
    int retval;

    init_lookupat(&lookup, dirfd, pathname, LKF_SYMLINK_NOFOLLOW, &vmnt,
                  &pin_dir);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;

    if ((pin_dir = last_dir(&lookup, fproc)) == NULL) return err_code;

    init_lookup(&lookup, lookup.pathname, 0, &vmnt_pin, &pin);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;
    pin = advance_path(pin_dir, &lookup, fproc);
    if (vmnt_pin) unlock_vmnt(vmnt_pin);

    if (!pin) {
        retval = ENOENT;
        goto out_put_dir;
    }

    if (!S_ISDIR(pin_dir->i_mode)) {
        retval = ENOTDIR;
        goto out_put_pin;
    }

    if ((retval = forbidden(fproc, pin_dir, W_BIT | X_BIT)) != 0)
        goto out_put_pin;

    if (flags & AT_REMOVEDIR)
        retval = request_rmdir(pin_dir->i_fs_ep, pin_dir->i_dev, pin_dir->i_num,
                               pathname);
    else
        retval = request_unlink(pin_dir->i_fs_ep, pin_dir->i_dev,
                                pin_dir->i_num, pathname);

    fsnotify_unlink(pin_dir, pin, pathname);

out_put_pin:
    if (pin_dir != pin) unlock_inode(pin_dir);
    put_inode(pin);

out_put_dir:
    unlock_inode(pin_dir);
    unlock_vmnt(vmnt);
    put_inode(pin_dir);

    return retval;
}

int do_unlink(void)
{
    char pathname[PATH_MAX];
    int flags, retval;

    if (self->msg_in.NAME_LEN >= PATH_MAX) return ENAMETOOLONG;

    /* fetch the name */
    if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                            self->msg_in.PATHNAME, self->msg_in.NAME_LEN)) !=
        OK)
        return retval;
    pathname[self->msg_in.NAME_LEN] = '\0';

    flags = self->msg_in.type == RMDIR ? AT_REMOVEDIR : 0;

    return common_unlink(AT_FDCWD, pathname, flags);
}

int do_unlinkat(void)
{
    int dirfd = self->msg_in.u.m_vfs_pathat.dirfd;
    int name_len = self->msg_in.u.m_vfs_pathat.name_len;
    int flags = self->msg_in.u.m_vfs_pathat.flags;
    char pathname[PATH_MAX];
    int retval;

    if (name_len >= PATH_MAX) return ENAMETOOLONG;

    if (flags & ~AT_REMOVEDIR) return EINVAL;

    /* fetch the name */
    if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                            self->msg_in.u.m_vfs_pathat.pathname, name_len)) !=
        OK)
        return retval;
    pathname[name_len] = '\0';

    return common_unlink(dirfd, pathname, flags);
}

static int common_rdlink(int dirfd, char* pathname, void* buf, size_t len)
{
    struct lookup lookup;
    struct vfs_mount* vmnt;
    struct inode* pin;
    int retval;

    init_lookupat(&lookup, dirfd, pathname, LKF_SYMLINK_NOFOLLOW, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_READ;
    pin = resolve_path(&lookup, fproc);

    if (!pin) {
        return -err_code;
    }

    if (!S_ISLNK(pin->i_mode)) {
        retval = -EINVAL;
    } else {
        retval = request_rdlink(pin->i_fs_ep, pin->i_dev, pin->i_num,
                                self->msg_in.source, buf, len);
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

int do_rdlink(void)
{
    void* buf = self->msg_in.BUF;
    size_t len = self->msg_in.BUF_LEN;
    char pathname[PATH_MAX];
    int retval;

    if (self->msg_in.NAME_LEN >= PATH_MAX) return -ENAMETOOLONG;

    /* fetch the name */
    if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                            self->msg_in.PATHNAME, self->msg_in.NAME_LEN)) !=
        OK)
        return -retval;
    pathname[self->msg_in.NAME_LEN] = '\0';

    return common_rdlink(AT_FDCWD, pathname, buf, len);
}

int do_rdlinkat(void)
{
    int dirfd = self->msg_in.u.m_vfs_pathat.dirfd;
    int name_len = self->msg_in.u.m_vfs_pathat.name_len;
    void* buf = self->msg_in.u.m_vfs_pathat.buf;
    size_t len = self->msg_in.u.m_vfs_pathat.buf_len;
    char pathname[PATH_MAX];
    int retval;

    if (name_len >= PATH_MAX) return -ENAMETOOLONG;

    /* fetch the name */
    if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                            self->msg_in.u.m_vfs_pathat.pathname, name_len)) !=
        OK)
        return -retval;
    pathname[name_len] = '\0';

    return common_rdlink(dirfd, pathname, buf, len);
}

/**
 * Send FTRUNC request to FS driver.
 * @param  fs_ep   Endpoint of the FS driver.
 * @param  dev     The device number.
 * @param  num     The inode number.
 * @param  newsize New size.
 * @return         Zero on success.
 */
static int request_ftrunc(endpoint_t fs_ep, dev_t dev, ino_t num, int newsize)
{
    MESSAGE m;

    m.type = FS_FTRUNC;
    m.REQ_DEV = (int)dev;
    m.REQ_NUM = (int)num;
    m.REQ_FILESIZE = newsize;

    fs_sendrec(fs_ep, &m);

    return m.RET_RETVAL;
}

int truncate_node(struct inode* pin, int newsize)
{
    if (!S_ISREG(pin->i_mode)) return EINVAL;
    int retval = request_ftrunc(pin->i_fs_ep, pin->i_dev, pin->i_num, newsize);
    if (retval == 0) {
        pin->i_size = newsize;
    }
    return retval;
}

static int request_symlink(endpoint_t fs_ep, dev_t dev, ino_t dir_ino,
                           char* last_component, endpoint_t src, void* target,
                           size_t target_len, struct fproc* fp)
{
    MESSAGE m;
    mgrant_id_t name_grant, target_grant;
    size_t name_len;

    name_len = strlen(last_component);
    name_grant =
        mgrant_set_direct(fs_ep, (vir_bytes)last_component, name_len, MGF_READ);
    if (name_grant == GRANT_INVALID)
        panic("vfs: request_rdlink failed to create name grant");

    target_grant =
        mgrant_set_proxy(fs_ep, src, (vir_bytes)target, target_len, MGF_READ);
    if (name_grant == GRANT_INVALID)
        panic("vfs: request_rdlink failed to create name grant");

    m.type = FS_SYMLINK;
    m.u.m_vfs_fs_symlink.dev = dev;
    m.u.m_vfs_fs_symlink.dir_ino = dir_ino;
    m.u.m_vfs_fs_symlink.name_grant = name_grant;
    m.u.m_vfs_fs_symlink.name_len = name_len;
    m.u.m_vfs_fs_symlink.target_grant = target_grant;
    m.u.m_vfs_fs_symlink.target_len = target_len;
    m.u.m_vfs_fs_symlink.uid = fp->effuid;
    m.u.m_vfs_fs_symlink.gid = fp->effgid;

    fs_sendrec(fs_ep, &m);

    mgrant_revoke(target_grant);
    mgrant_revoke(name_grant);

    return m.RETVAL;
}

int do_symlink(void)
{
    char pathname[PATH_MAX + 1];
    void *oldpath, *newpath;
    size_t oldpath_len, newpath_len;
    struct lookup lookup;
    struct inode* pin;
    struct vfs_mount* vmnt;
    int retval;

    oldpath = self->msg_in.u.m_vfs_link.old_path;
    newpath = self->msg_in.u.m_vfs_link.new_path;
    oldpath_len = self->msg_in.u.m_vfs_link.old_path_len;
    newpath_len = self->msg_in.u.m_vfs_link.new_path_len;

    if (oldpath_len == 0) {
        return ENOENT;
    } else if (oldpath_len > PATH_MAX) {
        return ENAMETOOLONG;
    }

    if (newpath_len > PATH_MAX) {
        return ENAMETOOLONG;
    }

    data_copy(SELF, pathname, fproc->endpoint, newpath, newpath_len);
    pathname[newpath_len] = 0;

    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;
    if ((pin = last_dir(&lookup, fproc)) == NULL) {
        return err_code;
    }

    retval = forbidden(fproc, pin, W_BIT | X_BIT);
    if (retval == 0) {
        retval = request_symlink(pin->i_fs_ep, pin->i_dev, pin->i_num, pathname,
                                 fproc->endpoint, oldpath, oldpath_len, fproc);
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}
