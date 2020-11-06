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
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <sys/syslimits.h>
#include <lyos/sysutils.h>

#include <libbdev/libbdev.h>
#include <libfsdriver/libfsdriver.h>

int fsdriver_register(const struct fsdriver* fsd)
{
    MESSAGE m;

    /* don't register this filesystem */
    if (!fsd->name) return 0;

    /* register the filesystem */
    m.type = FS_REGISTER;
    m.PATHNAME = fsd->name;
    m.NAME_LEN = strlen(fsd->name);
    send_recv(BOTH, TASK_FS, &m);

    return m.RETVAL;
}

int fsdriver_mountpoint(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->REQ_DEV;
    ino_t num = m->REQ_NUM;

    if (fsd->fs_mountpoint == NULL) return ENOSYS;

    return fsd->fs_mountpoint(dev, num);
}

int fsdriver_putinode(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->u.m_vfs_fs_putinode.dev;
    ino_t num = m->u.m_vfs_fs_putinode.num;
    unsigned int count = m->u.m_vfs_fs_putinode.count;

    if (fsd->fs_putinode == NULL) return ENOSYS;

    return fsd->fs_putinode(dev, num, count);
}

int fsdriver_create(const struct fsdriver* fsd, MESSAGE* m)
{
    mode_t mode = m->u.m_vfs_fs_create.mode;
    uid_t uid = m->u.m_vfs_fs_create.uid;
    gid_t gid = m->u.m_vfs_fs_create.gid;
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_create.dev;
    ino_t num = m->u.m_vfs_fs_create.num;
    size_t len = m->u.m_vfs_fs_create.name_len;
    char pathname[NAME_MAX + 1];
    int retval;

    if (fsd->fs_create == NULL) return ENOSYS;

    struct fsdriver_node fn;
    memset(&fn, 0, sizeof(fn));

    if ((retval = fsdriver_copy_name(src, m->u.m_vfs_fs_create.grant, len,
                                     pathname, NAME_MAX, TRUE)) != 0)
        return retval;

    retval = fsd->fs_create(dev, num, pathname, mode, uid, gid, &fn);
    if (retval) return retval;

    memset(m, 0, sizeof(MESSAGE));
    m->u.m_fs_vfs_create_reply.num = fn.fn_num;
    m->u.m_fs_vfs_create_reply.mode = fn.fn_mode;
    m->u.m_fs_vfs_create_reply.size = fn.fn_size;
    m->u.m_fs_vfs_create_reply.uid = fn.fn_uid;
    m->u.m_fs_vfs_create_reply.gid = fn.fn_gid;

    return retval;
}

int fsdriver_mkdir(const struct fsdriver* fsd, MESSAGE* m)
{
    mode_t mode = m->u.m_vfs_fs_create.mode;
    uid_t uid = m->u.m_vfs_fs_create.uid;
    gid_t gid = m->u.m_vfs_fs_create.gid;
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_create.dev;
    ino_t num = m->u.m_vfs_fs_create.num;
    size_t len = m->u.m_vfs_fs_create.name_len;
    char pathname[NAME_MAX + 1];
    int retval;

    if (fsd->fs_mkdir == NULL) return ENOSYS;

    if ((retval = fsdriver_copy_name(src, m->u.m_vfs_fs_create.grant, len,
                                     pathname, NAME_MAX, TRUE)) != 0)
        return retval;

    return fsd->fs_mkdir(dev, num, pathname, mode, uid, gid);
}

int fsdriver_mknod(const struct fsdriver* fsd, MESSAGE* m)
{
    mode_t mode = m->u.m_vfs_fs_mknod.mode;
    uid_t uid = m->u.m_vfs_fs_mknod.uid;
    gid_t gid = m->u.m_vfs_fs_mknod.gid;
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_mknod.dev;
    ino_t num = m->u.m_vfs_fs_mknod.num;
    size_t len = m->u.m_vfs_fs_mknod.name_len;
    dev_t sdev = m->u.m_vfs_fs_mknod.sdev;
    char pathname[NAME_MAX + 1];
    int retval;

    if (fsd->fs_mknod == NULL) return ENOSYS;

    if ((retval = fsdriver_copy_name(src, m->u.m_vfs_fs_mknod.grant, len,
                                     pathname, NAME_MAX, TRUE)) != 0)
        return retval;

    if (!strcmp(pathname, ".") || !strcmp(pathname, "..")) return EEXIST;

    return fsd->fs_mknod(dev, num, pathname, mode, uid, gid, sdev);
}

int fsdriver_readwrite(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->u.m_vfs_fs_readwrite.dev;
    ino_t num = m->u.m_vfs_fs_readwrite.num;
    loff_t position = m->u.m_vfs_fs_readwrite.position;
    size_t nbytes = m->u.m_vfs_fs_readwrite.count;
    int rw_flag = m->u.m_vfs_fs_readwrite.rw_flag;
    mgrant_id_t grant = m->u.m_vfs_fs_readwrite.grant;
    struct fsdriver_data data;
    ssize_t retval;

    data.granter = m->source;
    data.grant = grant;

    retval = -ENOSYS;
    if (rw_flag == READ && fsd->fs_read)
        retval = fsd->fs_read(dev, num, &data, position, nbytes);
    else if (rw_flag == WRITE && fsd->fs_write)
        retval = fsd->fs_write(dev, num, &data, position, nbytes);

    if (retval >= 0) {
        position += retval;

        m->u.m_vfs_fs_readwrite.position = position;
        m->u.m_vfs_fs_readwrite.count = retval;

        return 0;
    }

    return -retval;
}

int fsdriver_rdlink(const struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_data data;
    dev_t dev = m->u.m_vfs_fs_readwrite.dev;
    ino_t num = m->u.m_vfs_fs_readwrite.num;
    size_t size = m->u.m_vfs_fs_readwrite.count;
    endpoint_t user_endpt = m->u.m_vfs_fs_readwrite.user_endpt;
    ssize_t retval;

    if (!fsd->fs_rdlink) return ENOSYS;

    data.granter = m->source;
    data.grant = m->u.m_vfs_fs_readwrite.grant;

    retval = fsd->fs_rdlink(dev, num, &data, size, user_endpt);

    if (retval >= 0) {
        m->u.m_vfs_fs_readwrite.count = retval;
        return 0;
    }

    return -retval;
}

int fsdriver_stat(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->u.m_vfs_fs_stat.dev;
    ino_t num = m->u.m_vfs_fs_stat.num;
    struct fsdriver_data data;

    data.granter = m->source;
    data.grant = m->u.m_vfs_fs_stat.grant;

    if (fsd->fs_stat == NULL) return ENOSYS;

    return fsd->fs_stat(dev, num, &data);
}

int fsdriver_ftrunc(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = (dev_t)m->REQ_DEV;
    ino_t num = (ino_t)m->REQ_NUM;
    off_t start_pos = m->REQ_STARTPOS;
    off_t end_pos = m->REQ_ENDPOS;

    if (fsd->fs_ftrunc == NULL) return ENOSYS;

    return fsd->fs_ftrunc(dev, num, start_pos, end_pos);
}

int fsdriver_chmod(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = (dev_t)m->REQ_DEV;
    ino_t num = (ino_t)m->REQ_NUM;
    mode_t mode = m->REQ_MODE;

    if (fsd->fs_chmod == NULL) return ENOSYS;

    int retval = fsd->fs_chmod(dev, num, &mode);
    if (retval) return retval;

    m->RET_MODE = mode;

    return 0;
}

int fsdriver_chown(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->u.m_vfs_fs_chown.dev;
    ino_t num = m->u.m_vfs_fs_chown.num;
    uid_t uid = m->u.m_vfs_fs_chown.uid;
    gid_t gid = m->u.m_vfs_fs_chown.gid;
    mode_t mode;

    if (fsd->fs_chown == NULL) return ENOSYS;

    int retval = fsd->fs_chown(dev, num, uid, gid, &mode);
    if (retval) return retval;

    m->u.m_vfs_fs_chown.mode = mode;

    return 0;
}

int fsdriver_getdents(const struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_data data;
    dev_t dev = m->u.m_vfs_fs_readwrite.dev;
    ino_t num = m->u.m_vfs_fs_readwrite.num;
    loff_t position = m->u.m_vfs_fs_readwrite.position;
    size_t nbytes = m->u.m_vfs_fs_readwrite.count;
    ssize_t retval;

    data.granter = m->source;
    data.grant = m->u.m_vfs_fs_readwrite.grant;

    if (fsd->fs_getdents == NULL) return ENOSYS;

    retval = fsd->fs_getdents(dev, num, &data, &position, nbytes);
    if (retval >= 0) {
        m->u.m_vfs_fs_readwrite.position = position;
        m->u.m_vfs_fs_readwrite.count = retval;

        return 0;
    }

    return -retval;
}

int fsdriver_symlink(const struct fsdriver* fsd, MESSAGE* m)
{
    char name[NAME_MAX + 1];
    size_t name_len, target_len;
    struct fsdriver_data data;
    endpoint_t src = m->source;
    dev_t dev;
    ino_t dir_num;
    uid_t uid;
    gid_t gid;
    mgrant_id_t name_grant;
    int retval;

    if (!fsd->fs_symlink) return ENOSYS;

    dev = m->u.m_vfs_fs_symlink.dev;
    dir_num = m->u.m_vfs_fs_symlink.dir_ino;
    uid = m->u.m_vfs_fs_symlink.uid;
    gid = m->u.m_vfs_fs_symlink.gid;
    target_len = m->u.m_vfs_fs_symlink.target_len;
    name_len = m->u.m_vfs_fs_symlink.name_len;
    name_grant = m->u.m_vfs_fs_symlink.name_grant;

    if ((retval = fsdriver_copy_name(src, name_grant, name_len, name, NAME_MAX,
                                     TRUE)) != 0)
        return retval;

    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        return EEXIST;
    }

    data.granter = src;
    data.grant = m->u.m_vfs_fs_symlink.target_grant;

    return fsd->fs_symlink(dev, dir_num, name, uid, gid, &data, target_len);
}

int fsdriver_unlink(const struct fsdriver* fsd, MESSAGE* m)
{
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_unlink.dev;
    ino_t num = m->u.m_vfs_fs_unlink.num;
    mgrant_id_t grant = m->u.m_vfs_fs_unlink.grant;
    size_t len = m->u.m_vfs_fs_unlink.name_len;
    char name[NAME_MAX + 1];
    int retval;

    if (!fsd->fs_unlink) return ENOSYS;
    if ((retval = fsdriver_copy_name(src, grant, len, name, NAME_MAX, TRUE)) !=
        0)
        return retval;

    if (!strcmp(name, ".") || !strcmp(name, "..")) return EPERM;

    return fsd->fs_unlink(dev, num, name);
}

int fsdriver_rmdir(const struct fsdriver* fsd, MESSAGE* m)
{
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_unlink.dev;
    ino_t num = m->u.m_vfs_fs_unlink.num;
    mgrant_id_t grant = m->u.m_vfs_fs_unlink.grant;
    size_t len = m->u.m_vfs_fs_unlink.name_len;
    char name[NAME_MAX + 1];
    int retval;

    if (!fsd->fs_unlink) return ENOSYS;
    if ((retval = fsdriver_copy_name(src, grant, len, name, NAME_MAX, TRUE)) !=
        0)
        return retval;

    if (!strcmp(name, ".")) return EINVAL;
    if (!strcmp(name, "..")) return ENOTEMPTY;

    return fsd->fs_rmdir(dev, num, name);
}

int fsdriver_utime(const struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->u.m_vfs_fs_utime.dev;
    ino_t num = m->u.m_vfs_fs_utime.num;
    struct timespec atime, mtime;

    atime.tv_sec = m->u.m_vfs_fs_utime.atime;
    atime.tv_nsec = m->u.m_vfs_fs_utime.ansec;
    mtime.tv_sec = m->u.m_vfs_fs_utime.mtime;
    mtime.tv_nsec = m->u.m_vfs_fs_utime.mnsec;

    if (!fsd->fs_utime) return ENOSYS;

    return fsd->fs_utime(dev, num, &atime, &mtime);
}

int fsdriver_sync(const struct fsdriver* fsd, MESSAGE* m)
{
    if (fsd->fs_sync == NULL) return ENOSYS;
    return fsd->fs_sync();
}

int fsdriver_driver(dev_t dev) { return bdev_driver(dev); }
