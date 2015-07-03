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
#include <lyos/ipc.h>
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

PRIVATE struct inode * new_node(struct fproc* fp, char * pathname, int flags, mode_t mode);
PRIVATE int request_create(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid, gid_t gid,
                            char * pathname, mode_t mode, struct lookup_result * res);

/**
 * <Ring 1> Perform the open syscall.
 * @param  p Ptr to the message.
 * @return   fd number if success, otherwise a negative error code.
 */
PUBLIC int do_open(MESSAGE * p)
{
    int fd = -1;        /* return value */

    /* get parameters from the message */
    int flags = p->FLAGS;   /* open flags */
    int name_len = p->NAME_LEN; /* length of filename */
    int src = p->source;    /* caller proc nr. */
    mode_t mode = p->MODE;  /* access mode */
    struct fproc* pcaller = vfs_endpt_proc(src);
    
    mode_t bits = mode_map[flags & O_ACCMODE];
    if (!bits) return -EINVAL;

    int exist = 1;
    int retval = 0;

    char pathname[MAX_PATH];
    if (name_len > MAX_PATH) {
        err_code = -ENAMETOOLONG;
        return -1;
    }
        
    data_copy(TASK_FS, pathname, src, p->PATHNAME, name_len);
    pathname[name_len] = '\0';

    /* find a free slot in PROCESS::filp[] */
    int i;
    for (i = 0; i < NR_FILES; i++) {
        if (pcaller->filp[i] == 0) {
            fd = i;
            break;
        }
    }
    if ((fd < 0) || (fd >= NR_FILES))
        panic("filp[] is full (PID:%d)", pcaller->endpoint);

    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++)
        if (f_desc_table[i].fd_inode == 0)
            break;
    if (i >= NR_FILE_DESC)
        panic("f_desc_table[] is full (PID:%d)", pcaller->endpoint);

    struct inode * pin = NULL;

    if (flags & O_CREAT) {
        mode = I_REGULAR | (mode & ALL_MODES & pcaller->umask);
        err_code = 0;
        pin = new_node(pcaller, pathname, flags, mode);
        retval = err_code;
        if (retval == 0) exist = 0;
        else if (retval != EEXIST) {
            return -retval;
        } else exist = !(flags & O_EXCL);
    } else {
        pin = resolve_path(pathname, pcaller);
        if (pin == NULL) return -err_code;

        DEB(printl("open file `%s' with inode_nr = %d, proc: %d(%s)\n", pathname, pin->i_num, src, pcaller->name)); 
    }

    struct file_desc * filp = &f_desc_table[i];
    pcaller->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_pos = 0;
    filp->fd_inode = pin;
    filp->fd_mode = flags;

    MESSAGE driver_msg;
    if (exist) {
        if ((retval = forbidden(pcaller, pin, bits)) == 0) {
            switch (pin->i_mode & I_TYPE) {
                case I_REGULAR:
                    /* truncate the inode */
                    if (flags & O_TRUNC) {
                        if ((retval = forbidden(pcaller, pin, W_BIT)) != 0) {
                            break;
                        }
                        truncate_node(pin, 0);
                    }
                    break;
                case I_DIRECTORY:   /* directory may not be written */
                    retval = (bits & W_BIT) ? EISDIR : 0;
                    break;
                case I_CHAR_SPECIAL:    /* open char device */
                    driver_msg.type = DEV_OPEN;
                    int dev = pin->i_specdev;
                    driver_msg.DEVICE = MINOR(dev);
                    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
                    break;
                case I_BLOCK_SPECIAL:
                    /* TODO: handle block special file */
                    break;
                default:
                break;
            }
        }
    }

    if (retval != 0) {
        pcaller->filp[fd] = NULL;
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
    
    if (fd < 0 || fd >= NR_FILES) return EBADF;
    if (pcaller->filp[fd] == NULL) return EBADF;
    if (pcaller->filp[fd]->fd_inode == NULL) return EBADF;

    DEB(printl("closing file (filp[%d] of proc #%d, inode number = %d, fd->refcnt = %d, inode->refcnt = %d)\n", 
            fd, pcaller->endpoint, pcaller->filp[fd]->fd_inode->i_num, pcaller->filp[fd]->fd_cnt, pcaller->filp[fd]->fd_inode->i_cnt));
    put_inode(pcaller->filp[fd]->fd_inode);
    if (--pcaller->filp[fd]->fd_cnt == 0)
        pcaller->filp[fd]->fd_inode = NULL;
    pcaller->filp[fd] = NULL;

    return 0;
}

/**
 * <Ring 1> Create a new inode.
 * @param  pathname Pathname.
 * @param  flags    Open flags.
 * @param  mode     Mode.
 * @return          Ptr to the inode.
 */
PRIVATE struct inode * new_node(struct fproc* fp, char * pathname, int flags, mode_t mode)
{
    struct inode * pin_dir = NULL;

    struct lookup_result res;

    if ((pin_dir = last_dir(pathname, fp)) == NULL) return NULL;

    lock_inode(pin_dir);
    lock_vmnt(pin_dir->i_vmnt);
    int retval = 0;
    struct inode * pin = resolve_path(pathname, fp);

    /* no such entry, create one */
    if (pin == NULL && err_code == ENOENT) {
        if ((retval = forbidden(fp, pin_dir, W_BIT | X_BIT)) == 0) {
            retval = request_create(pin_dir->i_fs_ep, pin_dir->i_dev, pin_dir->i_num,
                fp->realuid, fp->realgid, pathname, mode, &res);
        }
        if (retval != 0) {
            err_code = retval;
            unlock_inode(pin_dir);
            unlock_vmnt(pin_dir->i_vmnt);
            put_inode(pin_dir);
            return NULL;
        }
        err_code = 0;
    } else {
        err_code = EEXIST;
        return NULL;
    }

    pin = new_inode(pin_dir->i_dev, res.inode_nr);
    pin->i_fs_ep = pin_dir->i_fs_ep;
    pin->i_size = res.size;
    pin->i_mode = res.mode;
    pin->i_uid = res.uid;
    pin->i_gid = res.gid;
    pin->i_vmnt = pin_dir->i_vmnt;
    pin->i_cnt = 1;

    unlock_inode(pin_dir);
    unlock_vmnt(pin_dir->i_vmnt);
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

    async_sendrec(fs_ep, &m, 0);

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

    struct file_desc * filp = pcaller->filp[fd];
    if (!filp) return EINVAL;

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

    return 0;
}
