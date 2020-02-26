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
#include "lyos/config.h"
#include "stdio.h"
#include <stdlib.h>
#include "stddef.h"
#include "errno.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "types.h"
#include "path.h"
#include "const.h"
#include "proto.h"
#include "global.h"

/**
 *   register_filesystem - register a new filesystem
 *   @m: Ptr to register message
 *
 *   Adds the file system passed to the list of file systems the kernel
 */
PUBLIC int do_register_filesystem(MESSAGE* p)
{
    int name_len = p->NAME_LEN;

    if (name_len > FS_LABEL_MAX) return ENAMETOOLONG;
    char name[FS_LABEL_MAX];
    name[name_len] = '\0';

    data_copy(SELF, name, p->source, p->PATHNAME, name_len);

    return add_filesystem(p->source, name);
}

PUBLIC int add_filesystem(endpoint_t fs_ep, char* name)
{
    struct file_system* pfs =
        (struct file_system*)malloc(sizeof(struct file_system));

    if (!pfs) {
        return ENOMEM;
    }

    strcpy(pfs->name, name);
    pfs->fs_ep = fs_ep;

    pthread_mutex_lock(&filesystem_lock);
    list_add(&(pfs->list), &filesystem_table);
    pthread_mutex_unlock(&filesystem_lock);

    printl("VFS: %s filesystem registered, endpoint: %d\n", pfs->name,
           pfs->fs_ep);

    return 0;
}

PUBLIC endpoint_t get_filesystem_endpoint(char* name)
{
    struct file_system* pfs;
    list_for_each_entry(pfs, &filesystem_table, list)
    {
        if (strcmp(pfs->name, name) == 0) {
            return pfs->fs_ep;
        }
    }
    return -1;
}

PUBLIC int request_readsuper(endpoint_t fs_ep, dev_t dev, int readonly,
                             int is_root, struct lookup_result* res)
{
    MESSAGE m;
    m.type = FS_READSUPER;
    m.REQ_FLAGS = 0;
    if (readonly) m.REQ_FLAGS |= RF_READONLY;
    if (is_root) m.REQ_FLAGS |= RF_ISROOT;

    m.REQ_DEV = dev;

    // async_sendrec(fs_ep, &m, 0);
    send_recv(BOTH, fs_ep, &m);

    int retval = m.RET_RETVAL;

    if (!retval) {
        res->dev = dev;
        res->fs_ep = fs_ep;
        res->inode_nr = m.RET_NUM;
        res->gid = m.RET_GID;
        res->uid = m.RET_UID;
        res->mode = m.RET_MODE;
        res->size = m.RET_FILESIZE;
    }
    return retval;
}
