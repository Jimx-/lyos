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

#include <lyos/type.h>
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
#include "types.h"
#include "errno.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

//#define OPEN_DEBUG
#ifdef OPEN_DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};

PRIVATE struct inode * new_node(struct fproc* fp, struct lookup* lookup, int flags, mode_t mode);
PRIVATE int request_create(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid, gid_t gid,
                            char * pathname, mode_t mode, struct lookup_result * res);

/**
 * <Ring 1> Perform the open syscall.
 * @param  p Ptr to the message.
 * @return   fd number if success, otherwise a negative error code.
 */
PUBLIC int do_open(MESSAGE * p)
{
    /* get parameters from the message */
    int flags = p->FLAGS;   /* open flags */
    int name_len = p->NAME_LEN; /* length of filename */
    int src = p->source;    /* caller proc nr. */
    mode_t mode = p->MODE;  /* access mode */
    struct fproc* pcaller = vfs_endpt_proc(src);

    char pathname[MAX_PATH];
    if (name_len > MAX_PATH) {
        return -ENAMETOOLONG;
    }
        
    data_copy(SELF, pathname, src, p->PATHNAME, name_len);
    pathname[name_len] = '\0';

    return common_open(pcaller, pathname, flags, mode);
}

PUBLIC int common_open(struct fproc* fp, char* pathname, int flags, mode_t mode)
{
    int fd = -1;    /* return fd */
    struct lookup lookup;
    mode_t bits = mode_map[flags & O_ACCMODE];
    if (!bits) return -EINVAL;

    int exist = 1;
    int retval = 0;

    /* find a free slot in PROCESS::filp[] */
    struct file_desc * filp = NULL;
    retval = get_fd(fp, 0, &fd, &filp);
    if (retval) return retval;

    struct inode* pin = NULL;
    struct vfs_mount* vmnt = NULL;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);

    if (flags & O_CREAT) {
        mode = I_REGULAR | (mode & ALL_MODES & fp->umask);
        err_code = 0;
        pin = new_node(fp, &lookup, flags, mode);
        retval = err_code;
        if (retval == 0) exist = 0;
        else if (retval != EEXIST) {
            if (pin) unlock_inode(pin);
            unlock_filp(filp);
            return -retval;
        } else exist = !(flags & O_EXCL);
    } else {
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_WRITE;

        pin = resolve_path(&lookup, fp);
        if (pin == NULL) {
            unlock_filp(filp);
            return -err_code;
        }

        if (vmnt) unlock_vmnt(vmnt);
    }

    fp->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_pos = 0;
    filp->fd_inode = pin;
    filp->fd_mode = flags;

    MESSAGE driver_msg;
    if (exist) {
        if ((retval = forbidden(fp, pin, bits)) == 0) {
            switch (pin->i_mode & I_TYPE) {
                case I_REGULAR:
                    /* truncate the inode */
                    if (flags & O_TRUNC) {
                        if ((retval = forbidden(fp, pin, W_BIT)) != 0) {
                            break;
                        }
                        truncate_node(pin, 0);
                    }
                    break;
                case I_DIRECTORY:   /* directory may not be written */
                    retval = (bits & W_BIT) ? EISDIR : 0;
                    break;
                case I_CHAR_SPECIAL:    /* open char device */
                {
                    int dev = pin->i_specdev;
                    retval = cdev_open(dev);
                    break;
                }
                case I_BLOCK_SPECIAL:
                    /* TODO: handle block special file */
                    break;
                default:
                break;
            }
        }
    }

    DEB(printl("open file `%s' with inode_nr = %d, proc: %d, %d\n", pathname, pin->i_num, fp->endpoint, fd)); 
    unlock_filp(filp);

    if (retval != 0) {
        fp->filp[fd] = NULL;
        filp->fd_cnt = 0;
        filp->fd_mode = 0;
        filp->fd_inode = 0;
        put_inode(pin);
        return -retval;
    }

    return fd;
}

/**
 * <Ring 1> Perform the close syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success.
 */
PUBLIC int do_close(MESSAGE * p)
{
    int fd = p->FD;
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    return close_fd(pcaller, fd);
}

PUBLIC int close_fd(struct fproc* fp, int fd)
{
    struct file_desc* filp = get_filp(fp, fd, RWL_WRITE);
    if (!filp) return EBADF;
    if (filp->fd_inode == NULL) {
        unlock_filp(filp);
        return EBADF;
    }

    DEB(printl("closing file (filp[%d] of proc #%d, inode number = %d, fd->refcnt = %d, inode->refcnt = %d)\n", fd, fp->endpoint, fp->filp[fd]->fd_inode->i_num, fp->filp[fd]->fd_cnt, fp->filp[fd]->fd_inode->i_cnt));

    struct inode* pin = filp->fd_inode;
    if (--filp->fd_cnt == 0) {
        unlock_inode(pin);
        put_inode(pin);
        filp->fd_inode = NULL;
    } else {
        unlock_inode(pin);
    }
    unlock_filp(filp);
    fp->filp[fd] = NULL;

    return 0;
}

/**
 * <Ring 1> Create a new inode.
 * @param  pathname Pathname.
 * @param  flags    Open flags.
 * @param  mode     Mode.
 * @return          Ptr to the inode.
 */
PRIVATE struct inode * new_node(struct fproc* fp, struct lookup* lookup, int flags, mode_t mode)
{
    struct inode * pin_dir = NULL;
    struct vfs_mount* vmnt_dir, *vmnt;
    struct lookup dir_lookup;
    struct lookup_result res;

    init_lookup(&dir_lookup, lookup->pathname, 0, &vmnt_dir, &pin_dir);
    dir_lookup.vmnt_lock = RWL_WRITE;
    dir_lookup.inode_lock = RWL_WRITE;
    if ((pin_dir = last_dir(&dir_lookup, fp)) == NULL) return NULL;
    if (vmnt_dir) unlock_vmnt(vmnt_dir);

    int retval = 0;
    struct inode * pin = NULL;
    init_lookup(&dir_lookup, dir_lookup.pathname, 0, &vmnt, &pin);
    dir_lookup.vmnt_lock = RWL_WRITE;
    dir_lookup.inode_lock = RWL_WRITE;
    pin = resolve_path(&dir_lookup, fp);
    if (vmnt) unlock_vmnt(vmnt);

    /* no such entry, create one */
    if (pin == NULL && err_code == ENOENT) {
        if ((retval = forbidden(fp, pin_dir, W_BIT | X_BIT)) == 0) {
            retval = request_create(pin_dir->i_fs_ep, pin_dir->i_dev, pin_dir->i_num,
                fp->realuid, fp->realgid, dir_lookup.pathname, mode, &res);
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
        if (pin_dir != pin)
            unlock_inode(pin_dir);
        put_inode(pin_dir);
        return pin;
    }

    pin = new_inode(pin_dir->i_dev, res.inode_nr);
    lock_inode(pin, RWL_WRITE);
    pin->i_fs_ep = pin_dir->i_fs_ep;
    pin->i_size = res.size;
    pin->i_mode = res.mode;
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
PRIVATE int request_create(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid, gid_t gid,
                            char * pathname, mode_t mode, struct lookup_result * res)
{
    MESSAGE m;

    m.type = FS_CREATE;
    m.CRDEV = (int)dev;
    m.CRINO = (int)num;
    m.CRUID = (int)uid;
    m.CRGID = (int)gid;
    m.CRPATHNAME = pathname;
    m.CRNAMELEN = strlen(pathname);
    m.CRMODE = (int)mode;

    //async_sendrec(fs_ep, &m, 0);
    send_recv(BOTH, fs_ep, &m);

    res->fs_ep = fs_ep;
    res->inode_nr = m.CRINO;
    res->mode = m.CRMODE;;
    res->uid = m.CRUID;
    res->gid = m.CRGID;
    res->size = m.CRFILESIZE;
    return m.CRRET;
}

/**
 * <Ring 1> Perform the LSEEK syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
PUBLIC int do_lseek(MESSAGE * p)
{
    int fd = p->FD;
    int offset = p->OFFSET;
    int whence = p->WHENCE;
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    struct file_desc * filp = get_filp(pcaller, fd, RWL_WRITE);
    if (!filp) return EBADF;

    u64 pos = 0;
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
    }

    u64 newpos = pos + offset;
    /* no negative position */
    if (newpos < 0) return EOVERFLOW;
    else {
        filp->fd_pos = newpos;
        p->OFFSET = newpos;
    }

    unlock_filp(filp);
    return 0;
}
