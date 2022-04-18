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
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include "types.h"
#include "const.h"
#include "path.h"
#include "global.h"
#include "proto.h"

/**
 * <Ring 1> Perform the access syscall.
 * @param  p Ptr to the message.
 * @return   Zero if success. Otherwise -1.
 */
int do_access(void)
{
    int namelen = self->msg_in.NAME_LEN + 1;
    char pathname[PATH_MAX + 1];
    if (namelen > PATH_MAX) return ENAMETOOLONG;

    data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME, namelen);
    pathname[namelen] = 0;

    struct lookup lookup;
    struct vfs_mount* vmnt = NULL;
    struct inode* pin = NULL;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_WRITE;
    pin = resolve_path(&lookup, fproc);
    if (!pin) return ENOENT;

    int retval = forbidden(fproc, pin, self->msg_in.MODE);

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

static int in_group(struct fproc* fp, gid_t grp)
{
    int i;

    for (i = 0; i < fp->ngroups; i++)
        if (fp->sgroups[i] == grp) return 0;

    return EINVAL;
}

int forbidden(struct fproc* fp, struct inode* pin, int access)
{
    mode_t bits, perm_bits;
    int shift;

    if (pin == NULL) return ENOENT;
    bits = pin->i_mode;

    if (fp->realuid == SU_UID) {
        if (S_ISDIR(bits) || bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
            perm_bits = R_BIT | W_BIT | X_BIT;
        else
            perm_bits = R_BIT | W_BIT;
    } else {
        if (fp->realuid == pin->i_uid)
            shift = 6; /* owner */
        else if (fp->realgid == pin->i_gid)
            shift = 3; /* group */
        else if (in_group(fp, pin->i_gid) == OK)
            shift = 3; /* supplementary groups */
        else
            shift = 0; /* other */
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
mode_t do_umask(void)
{
    mode_t old = ~(fproc->umask);
    fproc->umask = ~((mode_t)self->msg_in.MODE & RWX_MODES);
    return old;
}

static int request_chmod(endpoint_t fs_ep, dev_t dev, ino_t num, mode_t mode,
                         mode_t* result)
{
    MESSAGE m;
    m.type = FS_CHMOD;
    m.REQ_DEV = dev;
    m.REQ_NUM = num;
    m.REQ_MODE = mode;

    // async_sendrec(fs_ep, &m, 0);
    fs_sendrec(fs_ep, &m);

    *result = (mode_t)m.RET_MODE;

    return m.RET_RETVAL;
}

int do_chmod(int type)
{
    struct inode* pin = NULL;
    struct vfs_mount* vmnt = NULL;
    struct file_desc* filp = NULL;

    struct lookup lookup;
    if (type == CHMOD) {
        char pathname[PATH_MAX + 1];
        if (self->msg_in.NAME_LEN > PATH_MAX) return ENAMETOOLONG;

        /* fetch the name */
        data_copy(SELF, pathname, fproc->endpoint, self->msg_in.PATHNAME,
                  self->msg_in.NAME_LEN);
        pathname[self->msg_in.NAME_LEN] = '\0';

        init_lookup(&lookup, pathname, 0, &vmnt, &pin);
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_WRITE;
        pin = resolve_path(&lookup, fproc);
    } else if (type == FCHMOD) {
        filp = get_filp(fproc, self->msg_in.FD, RWL_WRITE);
        if (!filp) return EBADF;

        pin = filp->fd_inode;
    }

    if (!pin) {
        if (filp) unlock_filp(filp);
        return ENOENT;
    }

    if (pin->i_uid != fproc->effuid && fproc->effuid != SU_UID) {
        if (filp) unlock_filp(filp);
        return EPERM;
    }

    if (pin->i_vmnt->m_flags & VMNT_READONLY) {
        if (filp) unlock_filp(filp);
        return EROFS;
    }

    mode_t result;
    int retval = request_chmod(pin->i_fs_ep, pin->i_dev, pin->i_num,
                               self->msg_in.MODE, &result);

    if (retval == 0) pin->i_mode = result;

    if (type == CHMOD) {
        unlock_vmnt(vmnt);
        unlock_inode(pin);
    } else {
        unlock_filp(filp);
    }
    put_inode(pin);

    return retval;
}

int fs_getsetid(void)
{
    struct fproc* fp = vfs_endpt_proc(self->msg_in.ENDPOINT);
    int ngroups;
    int retval = 0;

    if (fproc->endpoint != TASK_PM) return EPERM;

    assert(fp != fproc);

    if (fp == NULL)
        retval = EINVAL;
    else {
        lock_fproc(fp);
        switch (self->msg_in.u.m3.m3i3) {
        case GS_SETUID:
            fp->realuid = self->msg_in.UID;
            fp->effuid = self->msg_in.EUID;
            break;
        case GS_SETGID:
            fp->realgid = self->msg_in.GID;
            fp->effgid = self->msg_in.EGID;
            break;
        case GS_SETGROUPS:
            ngroups = self->msg_in.CNT;

            if (ngroups * sizeof(gid_t) > sizeof(fp->sgroups))
                panic("vfs: %s too much gids to copy", __FUNCTION__);

            data_copy(SELF, fp->sgroups, fproc->endpoint, self->msg_in.BUF,
                      ngroups * sizeof(gid_t));
            fp->ngroups = ngroups;
            break;
        default:
            retval = EINVAL;
            break;
        }
        unlock_fproc(fp);
    }

    self->msg_out.type = PM_VFS_GETSETID_REPLY;
    self->msg_out.RETVAL = retval;
    self->msg_out.ENDPOINT = self->msg_in.ENDPOINT;
    send_recv(SEND, TASK_PM, &self->msg_out);

    return SUSPEND;
}

static int request_chown(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid,
                         gid_t gid, mode_t* new_mode)
{
    MESSAGE m;

    memset(&m, 0, sizeof(m));
    m.type = FS_CHOWN;
    m.u.m_vfs_fs_chown.dev = dev;
    m.u.m_vfs_fs_chown.num = num;
    m.u.m_vfs_fs_chown.uid = uid;
    m.u.m_vfs_fs_chown.gid = gid;

    fs_sendrec(fs_ep, &m);

    *new_mode = m.u.m_vfs_fs_chown.mode;

    return m.u.m_vfs_fs_chown.status;
}

int do_fchownat(int type)
{
    int fd = self->msg_in.u.m_vfs_fchownat.fd;
    int name_len = self->msg_in.u.m_vfs_fchownat.name_len;
    int flags = self->msg_in.u.m_vfs_fchownat.flags;
    struct lookup lookup;
    struct file_desc* filp;
    struct vfs_mount* vmnt;
    struct inode* pin;
    int lookup_flags;
    uid_t uid, new_uid;
    gid_t gid, new_gid;
    mode_t new_mode;
    char pathname[PATH_MAX + 1];
    int retval;

    uid = self->msg_in.u.m_vfs_fchownat.owner;
    gid = self->msg_in.u.m_vfs_fchownat.group;

    if (type == FCHOWNAT) {
        lookup_flags = 0;
        if (flags & AT_SYMLINK_NOFOLLOW) lookup_flags |= LKF_SYMLINK_NOFOLLOW;

        if (name_len > PATH_MAX) return ENAMETOOLONG;

        if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                                self->msg_in.u.m_vfs_fchownat.pathname,
                                name_len)) != OK)
            return retval;
        pathname[name_len] = 0;

        init_lookupat(&lookup, fd, pathname, lookup_flags, &vmnt, &pin);
        lookup.vmnt_lock = RWL_READ;
        lookup.inode_lock = RWL_WRITE;
        pin = resolve_path(&lookup, fproc);

        if (!pin) return err_code;
    } else {
        if ((filp = get_filp(fproc, fd, RWL_WRITE)) == NULL) return err_code;
        pin = filp->fd_inode;
        dup_inode(pin);
    }

    assert(pin);

    retval = 0;

    new_uid = (uid == (uid_t)-1) ? pin->i_uid : uid;
    new_gid = (gid == (uid_t)-1) ? pin->i_gid : gid;

    if (pin->i_vmnt && pin->i_vmnt->m_flags & VMNT_READONLY) retval = EROFS;

    if (retval == OK) {
        if (fproc->effuid != SU_UID) {
            /* The owner of a file may change the group of the file to any
             * group of which that owner is a member. */
            if (pin->i_uid != fproc->effuid) retval = EPERM;
            if (pin->i_uid != new_uid) retval = EPERM;
            if (fproc->effgid != new_gid) retval = EPERM;
        }
    }

    if (retval != OK) goto out;

    if ((retval = request_chown(pin->i_fs_ep, pin->i_dev, pin->i_num, new_uid,
                                new_gid, &new_mode)) == OK) {
        pin->i_uid = new_uid;
        pin->i_gid = new_gid;
        pin->i_mode = new_mode;
    }

out:
    if (type == FCHOWNAT) {
        unlock_inode(pin);
        unlock_vmnt(vmnt);
    } else {
        unlock_filp(filp);
    }
    put_inode(pin);

    return retval;
}
