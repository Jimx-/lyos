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
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/mgrant.h>
#include <fcntl.h>
#include <sys/syslimits.h>

#include "types.h"
#include "errno.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

//#define OPEN_DEBUG
#ifdef OPEN_DEBUG
#define DEB(x)       \
    printl("VFS: "); \
    x
#else
#define DEB(x)
#endif

static char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};

static struct inode* new_node(struct fproc* fp, struct lookup* lookup,
                              int flags, mode_t mode);
static int request_create(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid,
                          gid_t gid, char* pathname, mode_t mode,
                          struct lookup_result* res);

/**
 * <Ring 1> Perform the open syscall.
 * @param  p Ptr to the message.
 * @return   fd number if success, otherwise a negative error code.
 */
int do_open(void)
{
    /* get parameters from the message */
    int flags = self->msg_in.FLAGS;       /* open flags */
    int name_len = self->msg_in.NAME_LEN; /* length of filename */
    int src = self->msg_in.source;        /* caller proc nr. */
    mode_t mode = self->msg_in.MODE;      /* access mode */

    char pathname[PATH_MAX];
    if (name_len > PATH_MAX) {
        return -ENAMETOOLONG;
    }

    data_copy(SELF, pathname, src, self->msg_in.PATHNAME, name_len);
    pathname[name_len] = '\0';

    return common_open(pathname, flags, mode);
}

int common_open(char* pathname, int flags, mode_t mode)
{
    int fd = -1; /* return fd */
    struct lookup lookup;
    mode_t bits = mode_map[flags & O_ACCMODE];
    if (!bits) return -EINVAL;

    int exist = 1;
    int retval = 0;

    /* find a free slot in PROCESS::filp[] */
    struct file_desc* filp = NULL;
    retval = get_fd(fproc, 0, &fd, &filp);
    if (retval) return retval;

    struct inode* pin = NULL;
    struct vfs_mount* vmnt = NULL;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);

    if (flags & O_CREAT) {
        mode = S_IFREG | (mode & ALL_MODES & fproc->umask);
        err_code = 0;
        pin = new_node(fproc, &lookup, flags, mode);
        retval = err_code;
        if (retval == 0)
            exist = 0;
        else if (retval != EEXIST) {
            if (pin) unlock_inode(pin);
            unlock_filp(filp);
            return -retval;
        } else
            exist = !(flags & O_EXCL);
    } else {
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_WRITE;

        pin = resolve_path(&lookup, fproc);
        if (pin == NULL) {
            unlock_filp(filp);
            return -err_code;
        }

        if (vmnt) unlock_vmnt(vmnt);
    }

    fproc->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_pos = 0;
    filp->fd_inode = pin;
    filp->fd_mode = flags;
    filp->fd_fops = pin->i_fops;

    if (exist) {
        if ((retval = forbidden(fproc, pin, bits)) == 0) {
            if (S_ISREG(pin->i_mode)) {
                /* truncate the inode */
                if (flags & O_TRUNC) {
                    if ((retval = forbidden(fproc, pin, W_BIT)) == OK) {
                        truncate_node(pin, 0);
                    }
                }
            } else if (S_ISDIR(pin->i_mode)) {
                retval = (bits & W_BIT) ? EISDIR : 0;
            } else if (S_ISCHR(pin->i_mode)) {
                retval = filp->fd_fops->open(pin, filp);
            } else {
                /* TODO: handle other file types */
            }
        }
    }

    DEB(printl("open file `%s' with inode_nr = %d, proc: %d, %d\n", pathname,
               pin->i_num, fproc->endpoint, fd));
    unlock_filp(filp);

    if (retval != 0) {
        fproc->filp[fd] = NULL;
        filp->fd_cnt = 0;
        filp->fd_mode = 0;
        filp->fd_inode = 0;
        filp->fd_fops = NULL;
        put_inode(pin);
        return -retval;
    }

    return fd;
}

int close_fd(struct fproc* fp, int fd)
{
    struct file_desc* filp = get_filp(fp, fd, RWL_WRITE);
    struct inode* pin;

    if (!filp) return EBADF;

    if (filp->fd_inode == NULL) {
        unlock_filp(filp);
        return EBADF;
    }

    DEB(printl("closing file (filp[%d] of proc #%d, inode number = %d, "
               "fd->refcnt = %d, inode->refcnt = %d)\n",
               fd, fp->endpoint, fp->filp[fd]->fd_inode->i_num,
               fp->filp[fd]->fd_cnt, fp->filp[fd]->fd_inode->i_cnt));

    pin = filp->fd_inode;

    if (--filp->fd_cnt == 0) {
        if (filp->fd_fops && filp->fd_fops->release) {
            filp->fd_fops->release(pin, filp);
        }

        unlock_inode(pin);
        put_inode(pin);
        filp->fd_inode = NULL;
    } else if (filp->fd_cnt < 0) {
        panic("VFS: invalid filp ref count");
    } else {
        unlock_inode(pin);
    }

    mutex_unlock(&filp->fd_lock);
    fp->filp[fd] = NULL;

    return 0;
}

/**
 * <Ring 1> Perform the close syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success.
 */
int do_close()
{
    int fd = self->msg_in.FD;

    return close_fd(fproc, fd);
}

/**
 * <Ring 1> Create a new inode.
 * @param  pathname Pathname.
 * @param  flags    Open flags.
 * @param  mode     Mode.
 * @return          Ptr to the inode.
 */
static struct inode* new_node(struct fproc* fp, struct lookup* lookup,
                              int flags, mode_t mode)
{
    struct inode* pin_dir = NULL;
    struct vfs_mount *vmnt_dir, *vmnt;
    struct lookup dir_lookup;
    struct lookup_result res;

    init_lookup(&dir_lookup, lookup->pathname, 0, &vmnt_dir, &pin_dir);
    dir_lookup.vmnt_lock = RWL_WRITE;
    dir_lookup.inode_lock = RWL_WRITE;
    if ((pin_dir = last_dir(&dir_lookup, fp)) == NULL) return NULL;
    if (vmnt_dir) unlock_vmnt(vmnt_dir);

    int retval = 0;
    struct inode* pin = NULL;
    init_lookup(&dir_lookup, dir_lookup.pathname, 0, &vmnt, &pin);
    dir_lookup.vmnt_lock = RWL_WRITE;
    dir_lookup.inode_lock = RWL_WRITE;
    pin = advance_path(pin_dir, &dir_lookup, fp);
    if (vmnt) unlock_vmnt(vmnt);

    /* no such entry, create one */
    if (pin == NULL && err_code == ENOENT) {
        if ((retval = forbidden(fp, pin_dir, W_BIT | X_BIT)) == 0) {
            retval = request_create(pin_dir->i_fs_ep, pin_dir->i_dev,
                                    pin_dir->i_num, fp->realuid, fp->realgid,
                                    dir_lookup.pathname, mode, &res);
        }
        if (retval != 0) {
            err_code = retval;
            unlock_inode(pin_dir);
            put_inode(pin_dir);
            return NULL;
        }
        err_code = 0;
    } else {
        err_code = EEXIST;
        if (pin_dir != pin) unlock_inode(pin_dir);
        put_inode(pin_dir);
        return pin;
    }

    pin = new_inode(pin_dir->i_dev, res.inode_nr, res.mode);
    lock_inode(pin, RWL_WRITE);
    pin->i_fs_ep = pin_dir->i_fs_ep;
    pin->i_size = res.size;
    pin->i_uid = res.uid;
    pin->i_gid = res.gid;
    pin->i_vmnt = pin_dir->i_vmnt;
    pin->i_cnt = 1;

    unlock_inode(pin_dir);
    put_inode(pin_dir);

    return pin;
}

/**
 * <Ring 1> Issue the create request.
 * @param  fs_ep    Filesystem endpoint.
 * @param  dev      Device number.
 * @param  num      Inode number.
 * @param  uid      UID of the caller.
 * @param  gid      GID of the caller.
 * @param  pathname The pathname.
 * @param  mode     Inode mode.
 * @param  res      Result.
 * @return          Zero on success.
 */
static int request_create(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid,
                          gid_t gid, char* pathname, mode_t mode,
                          struct lookup_result* res)
{
    MESSAGE m;
    size_t name_len;
    mgrant_id_t grant;
    int retval;

    name_len = strlen(pathname);
    grant = mgrant_set_direct(fs_ep, (vir_bytes)pathname, name_len, MGF_READ);
    if (grant == GRANT_INVALID)
        panic("vfs: request_create cannot create grant");

    m.type = FS_CREATE;
    m.u.m_vfs_fs_create.dev = dev;
    m.u.m_vfs_fs_create.num = num;
    m.u.m_vfs_fs_create.uid = uid;
    m.u.m_vfs_fs_create.gid = gid;
    m.u.m_vfs_fs_create.grant = grant;
    m.u.m_vfs_fs_create.name_len = name_len;
    m.u.m_vfs_fs_create.mode = mode;

    fs_sendrec(fs_ep, &m);
    retval = m.u.m_fs_vfs_create_reply.status;
    mgrant_revoke(grant);

    if (retval) return retval;

    res->fs_ep = fs_ep;
    res->inode_nr = m.u.m_fs_vfs_create_reply.num;
    res->mode = m.u.m_fs_vfs_create_reply.mode;
    res->uid = m.u.m_fs_vfs_create_reply.uid;
    res->gid = m.u.m_fs_vfs_create_reply.gid;
    res->size = m.u.m_fs_vfs_create_reply.size;

    return 0;
}

/**
 * <Ring 1> Perform the LSEEK syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
int do_lseek(void)
{
    int fd = self->msg_in.FD;
    int offset = self->msg_in.OFFSET;
    int whence = self->msg_in.WHENCE;
    off_t pos, newpos;

    struct file_desc* filp = get_filp(fproc, fd, RWL_WRITE);
    if (!filp) return EBADF;

    switch (whence) {
    case SEEK_SET:
        pos = 0;
        break;
    case SEEK_CUR:
        pos = filp->fd_pos;
        break;
    case SEEK_END:
        pos = filp->fd_inode->i_size;
        break;
    default:
        unlock_filp(filp);
        return EINVAL;
    }

    newpos = pos + offset;
    if (((offset > 0) && (newpos <= pos)) ||
        ((offset < 0) && (newpos >= pos))) {
        unlock_filp(filp);
        return EOVERFLOW;
    } else {
        filp->fd_pos = newpos;
        self->msg_out.OFFSET = newpos;
    }

    unlock_filp(filp);
    return 0;
}

static int request_mkdir(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid,
                         gid_t gid, char* pathname, mode_t mode)
{
    MESSAGE m;
    size_t name_len;
    mgrant_id_t grant;

    name_len = strlen(pathname);
    grant = mgrant_set_direct(fs_ep, (vir_bytes)pathname, name_len, MGF_READ);
    if (grant == GRANT_INVALID) panic("vfs: request_mkdir cannot create grant");

    memset(&m, 0, sizeof(MESSAGE));
    m.type = FS_MKDIR;
    m.u.m_vfs_fs_create.dev = dev;
    m.u.m_vfs_fs_create.num = num;
    m.u.m_vfs_fs_create.uid = uid;
    m.u.m_vfs_fs_create.gid = gid;
    m.u.m_vfs_fs_create.grant = grant;
    m.u.m_vfs_fs_create.name_len = name_len;
    m.u.m_vfs_fs_create.mode = mode;

    fs_sendrec(fs_ep, &m);

    mgrant_revoke(grant);

    return m.u.m_fs_vfs_create_reply.status;
}

int do_mkdir(void)
{
    int name_len = self->msg_in.NAME_LEN; /* length of filename */
    mode_t mode = self->msg_in.MODE;      /* access mode */

    char pathname[PATH_MAX + 1];
    if (name_len > PATH_MAX) {
        return -ENAMETOOLONG;
    }

    data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME, name_len);
    pathname[name_len] = '\0';

    struct vfs_mount* vmnt;
    struct inode* pin;
    struct lookup lookup;
    int retval = 0;

    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;

    mode_t bits = S_IFDIR | (mode & ALL_MODES & fproc->umask);
    if ((pin = last_dir(&lookup, fproc)) == NULL) {
        return errno;
    }

    if (!S_ISDIR(pin->i_mode)) {
        retval = ENOTDIR;
    } else if ((retval = forbidden(fproc, pin, W_BIT | X_BIT)) == 0) {
        retval =
            request_mkdir(pin->i_fs_ep, pin->i_dev, pin->i_num, fproc->realuid,
                          fproc->realgid, lookup.pathname, bits);
    }

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}
