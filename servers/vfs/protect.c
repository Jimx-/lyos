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
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "path.h"
#include "global.h"
#include "proto.h"
#include <sys/stat.h>

/**
 * <Ring 1> Perform the access syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success. Otherwise -1.
 */
PUBLIC int do_access(MESSAGE * p)
{
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    int namelen = p->NAME_LEN + 1;
    char pathname[MAX_PATH];
    if (namelen > MAX_PATH) return ENAMETOOLONG;

    data_copy(SELF, pathname, p->source, p->PATHNAME, namelen);
    //phys_copy(va2pa(getpid(), pathname), va2pa(p->source, p->PATHNAME), namelen);
    pathname[namelen] = 0;

    struct inode * pin = resolve_path(pathname, pcaller);
    if (!pin) return ENOENT;

    int retval = forbidden(pcaller, pin, p->MODE);

    put_inode(pin);

    return (retval == 0) ? 0 : -1;
}

PUBLIC int forbidden(struct fproc * fp, struct inode * pin, int access)
{
    mode_t bits, perm_bits;
    int shift;

    if (pin == NULL) return ENOENT;
    bits = pin->i_mode;

    if (fp->realuid == SU_UID) {
        if ((bits & I_TYPE) == I_DIRECTORY || bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
            perm_bits = R_BIT | W_BIT | X_BIT;
        else
            perm_bits = R_BIT | W_BIT;
    } else {
        if (fp->realuid == pin->i_uid) shift = 6;    /* owner */
        else if (fp->realgid == pin->i_gid) shift = 3;   /* group */
        else shift = 0;                 /* other */
        perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT); 
    }

    if ((perm_bits | access) != perm_bits) return EACCES;

    /* check for readonly filesystem */
    if (access & W_BIT) {
        if (pin->i_vmnt->m_flags & VMNT_READONLY) return EROFS;
    }

    return 0;
}

/**
 * <Ring 1> Perform the UMASK syscall.
 * @param  p Ptr to the message.
 * @return   Complement of the old mask.
 */
PUBLIC mode_t do_umask(MESSAGE * p)
{
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    mode_t old = ~(pcaller->umask);
    pcaller->umask = ~((mode_t)p->MODE & RWX_MODES);
    return old;
}

PRIVATE int request_chmod(endpoint_t fs_ep, dev_t dev, ino_t num, mode_t mode, mode_t * result)
{
    MESSAGE m;
    m.type = FS_CHMOD;
    m.REQ_DEV = dev;
    m.REQ_NUM = num;
    m.REQ_MODE = mode;

    async_sendrec(fs_ep, &m, 0);

    *result = (mode_t)m.RET_MODE;

    return m.RET_RETVAL;
}

PUBLIC int do_chmod(int type, MESSAGE * p)
{
    struct inode * pin;
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    if (type == CHMOD) {
        char pathname[MAX_PATH];
        if (p->NAME_LEN > MAX_PATH) return ENAMETOOLONG;

        /* fetch the name */
        data_copy(SELF, pathname, pcaller->endpoint, p->PATHNAME, p->NAME_LEN);
        pathname[p->NAME_LEN] = '\0';

        pin = resolve_path(pathname, pcaller);
    } else if (type == FCHMOD) {
        struct file_desc * filp = pcaller->filp[p->FD];
        if (!filp) return EBADF;

        pin = filp->fd_inode;
    }

    if (!pin) return ENOENT;

    if (pin->i_uid != pcaller->effuid && pcaller->effuid != SU_UID) return EPERM;
    if (pin->i_vmnt->m_flags & VMNT_READONLY) return EROFS;

    mode_t result;
    int retval = request_chmod(pin->i_fs_ep, pin->i_dev, pin->i_num, p->MODE, &result);

    if (retval == 0) pin->i_mode = result;

    put_inode(pin);

    return 0;
}

PUBLIC int fs_getsetid(MESSAGE * p)
{
    if (p->source != TASK_PM) return EPERM;

    struct fproc * fp = vfs_endpt_proc(p->ENDPOINT);
    int retval = 0;

    if (fp == NULL) 
        retval = EINVAL;
    else {
        switch (p->u.m3.m3i3) {
        case GS_SETUID:
            fp->realuid = p->UID;
            fp->effuid = p->EUID;
            break;
        case GS_SETGID:
            fp->realgid = p->GID;
            fp->effgid = p->EGID;
            break;
        default:
            retval = EINVAL;
            break;
        }
    }

    p->type = PM_VFS_GETSETID_REPLY;
    send_recv(SEND, TASK_PM, p);

    return SUSPEND;
}
