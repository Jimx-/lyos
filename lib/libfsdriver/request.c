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
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/ipc.h>
#include <sys/syslimits.h>
#include "libfsdriver/libfsdriver.h"

PUBLIC int fsdriver_register(struct fsdriver * fsd)
{
    MESSAGE m;

    /* register the filesystem */
    m.type = FS_REGISTER;
    m.PATHNAME = fsd->name;
    m.NAME_LEN = strlen(fsd->name);
    send_recv(BOTH, TASK_FS, &m);

    return m.RETVAL;
}

PUBLIC int fsdriver_readsuper(struct fsdriver * fsd, MESSAGE * m)
{
    struct fsdriver_node fn;
    dev_t dev = m->REQ_DEV;
    int retval = fsd->fs_readsuper(dev, m->REQ_FLAGS, &fn);
    if (retval) return retval;

    m->RET_NUM = fn.fn_num;
    m->RET_UID = fn.fn_uid;
    m->RET_GID = fn.fn_gid;
    m->RET_FILESIZE = fn.fn_size;
    m->RET_MODE = fn.fn_mode;

    return retval;
}

PUBLIC int fsdriver_putinode(struct fsdriver * fsd, MESSAGE * m)
{
    dev_t dev = m->REQ_DEV;
    ino_t num = m->REQ_NUM;

    return fsd->fs_putinode(dev, num);
}

PUBLIC int fsdriver_create(struct fsdriver * fsd, MESSAGE * m)
{
    mode_t mode = (mode_t)(m->CRMODE);
    uid_t uid = (uid_t)(m->CRUID);
    gid_t gid = (gid_t)(m->CRGID);
    int src = m->source;

    dev_t dev = m->CRDEV;
    ino_t num = (ino_t)(m->CRINO);

    struct fsdriver_node fn;
    memset(&fn, 0, sizeof(fn));

    char pathname[NAME_MAX + 1];
    int len = m->CRNAMELEN;

    /* error: name too long */
    if (len > NAME_MAX + 1) {
        return ENAMETOOLONG;
    }

    data_copy(SELF, pathname, src, m->CRPATHNAME, len);
    pathname[len] = '\0';
    
    int retval = fsd->fs_create(dev, num, pathname, mode, uid, gid, &fn);
    if (retval) return retval;

    memset(m, 0, sizeof(MESSAGE));
    m->CRINO = fn.fn_num;
    m->CRMODE = fn.fn_mode;
    m->CRFILESIZE = fn.fn_size;
    m->CRUID = fn.fn_uid;
    m->CRGID = fn.fn_gid;

    return retval;
}

PUBLIC int fsdriver_stat(struct fsdriver * fsd, MESSAGE * m)
{
    dev_t dev = (dev_t)m->STDEV;
    ino_t num = (ino_t)m->STINO;
    struct fsdriver_data data;
    data.src = m->STSRC;
    data.buf = m->STBUF;

    return fsd->fs_stat(dev, num, &data);
}

PUBLIC int fsdriver_ftrunc(struct fsdriver * fsd, MESSAGE * m)
{
    dev_t dev = (dev_t)m->REQ_DEV;
    ino_t num = (ino_t)m->REQ_INO;
    off_t start_pos = m->REQ_STARTPOS;
    off_t end_pos = m->REQ_ENDPOS;

    return fsd->fs_ftrunc(dev, num, start_pos, end_pos);
}

PUBLIC int fsdriver_sync(struct fsdriver * fsd, MESSAGE * m)
{
    return fsd->fs_sync();
}
