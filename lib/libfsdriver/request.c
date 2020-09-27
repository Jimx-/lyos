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

int fsdriver_register(struct fsdriver* fsd)
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

int fsdriver_readsuper(struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_node fn;
    dev_t dev = m->REQ_DEV;

    if (fsd->fs_readsuper == NULL) return ENOSYS;

    if (fsd->fs_driver) fsd->fs_driver(dev);

    int retval = fsd->fs_readsuper(dev, m->REQ_FLAGS, &fn);
    if (retval) return retval;

    m->RET_NUM = fn.fn_num;
    m->RET_UID = fn.fn_uid;
    m->RET_GID = fn.fn_gid;
    m->RET_FILESIZE = fn.fn_size;
    m->RET_MODE = fn.fn_mode;

    return retval;
}

int fsdriver_mountpoint(struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->REQ_DEV;
    ino_t num = m->REQ_NUM;

    if (fsd->fs_mountpoint == NULL) return ENOSYS;

    return fsd->fs_mountpoint(dev, num);
}

int fsdriver_putinode(struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = m->REQ_DEV;
    ino_t num = m->REQ_NUM;

    if (fsd->fs_putinode == NULL) return ENOSYS;

    return fsd->fs_putinode(dev, num);
}

int fsdriver_create(struct fsdriver* fsd, MESSAGE* m)
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
    m->u.m_vfs_fs_create_reply.num = fn.fn_num;
    m->u.m_vfs_fs_create_reply.mode = fn.fn_mode;
    m->u.m_vfs_fs_create_reply.size = fn.fn_size;
    m->u.m_vfs_fs_create_reply.uid = fn.fn_uid;
    m->u.m_vfs_fs_create_reply.gid = fn.fn_gid;

    return retval;
}

int fsdriver_mkdir(struct fsdriver* fsd, MESSAGE* m)
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

int fsdriver_readwrite(struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = (int)m->RWDEV;
    ino_t num = m->RWINO;
    loff_t rwpos = m->RWPOS;
    int src = m->RWSRC;
    int rw_flag = m->RWFLAG;
    void* buf = m->RWBUF;
    size_t nbytes = m->RWCNT;

    if (fsd->fs_readwrite == NULL) return ENOSYS;

    struct fsdriver_data data;
    data.src = src;
    data.buf = buf;

    ssize_t retval = fsd->fs_readwrite(dev, num, rw_flag, &data, rwpos, nbytes);

    if (retval >= 0) {
        rwpos += retval;

        m->RWPOS = rwpos;
        m->RWCNT = retval;

        return 0;
    }

    return -retval;
}

int fsdriver_rdlink(struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_data data;
    dev_t dev = m->RWDEV;
    ino_t num = m->RWINO;
    size_t size = m->RWCNT;
    ssize_t retval;

    if (!fsd->fs_rdlink) return ENOSYS;

    data.src = m->RWSRC;
    data.buf = m->RWBUF;

    retval = fsd->fs_rdlink(dev, num, &data, size);

    if (retval >= 0) {
        m->RWCNT = retval;
        return 0;
    }

    return -retval;
}

int fsdriver_stat(struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = (dev_t)m->STDEV;
    ino_t num = (ino_t)m->STINO;
    struct fsdriver_data data;
    data.src = m->STSRC;
    data.buf = m->STBUF;

    if (fsd->fs_stat == NULL) return ENOSYS;

    return fsd->fs_stat(dev, num, &data);
}

int fsdriver_ftrunc(struct fsdriver* fsd, MESSAGE* m)
{
    dev_t dev = (dev_t)m->REQ_DEV;
    ino_t num = (ino_t)m->REQ_NUM;
    off_t start_pos = m->REQ_STARTPOS;
    off_t end_pos = m->REQ_ENDPOS;

    if (fsd->fs_ftrunc == NULL) return ENOSYS;

    return fsd->fs_ftrunc(dev, num, start_pos, end_pos);
}

int fsdriver_chmod(struct fsdriver* fsd, MESSAGE* m)
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

int fsdriver_getdents(struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_data data;
    dev_t dev = m->RWDEV;
    ino_t num = m->RWINO;
    loff_t position = m->RWPOS;
    size_t nbytes = m->RWCNT;
    ssize_t retval;

    data.src = m->RWSRC;
    data.buf = m->RWBUF;

    if (fsd->fs_getdents == NULL) return ENOSYS;

    retval = fsd->fs_getdents(dev, num, &data, &position, nbytes);
    if (retval >= 0) {
        m->RWPOS = position;
        m->RWCNT = retval;

        return 0;
    }

    return -retval;
}

int fsdriver_symlink(struct fsdriver* fsd, MESSAGE* m)
{
    char name[NAME_MAX + 1];
    size_t name_len, target_len;
    struct fsdriver_data data;
    dev_t dev;
    ino_t dir_num;
    uid_t uid;
    gid_t gid;

    if (!fsd->fs_symlink) return ENOSYS;

    dev = m->u.m_vfs_fs_symlink.dev;
    dir_num = m->u.m_vfs_fs_symlink.dir_ino;
    uid = m->u.m_vfs_fs_symlink.uid;
    gid = m->u.m_vfs_fs_symlink.gid;
    target_len = m->u.m_vfs_fs_symlink.target_len;
    name_len = m->u.m_vfs_fs_symlink.name_len;

    if (name_len > NAME_MAX) {
        return ENAMETOOLONG;
    }

    data_copy(SELF, name, m->source, m->u.m_vfs_fs_symlink.name, name_len);
    name[name_len] = '\0';

    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        return EEXIST;
    }

    data.src = m->u.m_vfs_fs_symlink.src;
    data.buf = m->u.m_vfs_fs_symlink.target;

    return fsd->fs_symlink(dev, dir_num, name, uid, gid, &data, target_len);
}

int fsdriver_sync(struct fsdriver* fsd, MESSAGE* m)
{
    if (fsd->fs_sync == NULL) return ENOSYS;
    return fsd->fs_sync();
}

int fsdriver_driver(dev_t dev) { return bdev_driver(dev); }
