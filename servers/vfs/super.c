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
#include <stdlib.h>
#include "stddef.h"
#include <stdint.h>
#include "errno.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>
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
int do_register_filesystem()
{
    int name_len = self->msg_in.NAME_LEN;

    if (name_len > FS_LABEL_MAX) return ENAMETOOLONG;
    char name[FS_LABEL_MAX + 1];

    data_copy(SELF, name, fproc->endpoint, self->msg_in.PATHNAME, name_len);
    name[name_len] = '\0';

    return add_filesystem(fproc->endpoint, name);
}

int add_filesystem(endpoint_t fs_ep, char* name)
{
    struct file_system* pfs =
        (struct file_system*)malloc(sizeof(struct file_system));

    if (!pfs) {
        return ENOMEM;
    }

    strlcpy(pfs->name, name, sizeof(pfs->name));
    pfs->fs_ep = fs_ep;

    list_add(&(pfs->list), &filesystem_table);

    printl("VFS: %s filesystem registered, endpoint: %d\n", pfs->name,
           pfs->fs_ep);

    return 0;
}

endpoint_t get_filesystem_endpoint(char* name)
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

int request_readsuper(endpoint_t fs_ep, dev_t dev, int readonly, int is_root,
                      endpoint_t user_endpt, void* user_data,
                      struct lookup_result* res)
{
    MESSAGE m;
    mgrant_id_t grant;
    size_t data_size;
    int flags = 0;

    memset(&m, 0, sizeof(m));

    if (readonly) flags |= RF_READONLY;
    if (is_root) flags |= RF_ISROOT;

    grant = GRANT_INVALID;
    data_size = 0;
    if (user_data) {
        grant = mgrant_set_proxy(fs_ep, user_endpt, (vir_bytes)user_data,
                                 ARCH_PG_SIZE, MGF_READ);
        if (grant == GRANT_INVALID)
            panic("vfs: %s failed to create proxy grant", __FUNCTION__);

        /* at least the rest of the page is mapped */
        data_size = ARCH_PG_SIZE - ((uintptr_t)user_data % ARCH_PG_SIZE);
    }

    m.type = FS_READSUPER;
    m.u.m_vfs_fs_readsuper.dev = dev;
    m.u.m_vfs_fs_readsuper.flags = flags;
    m.u.m_vfs_fs_readsuper.grant = grant;
    m.u.m_vfs_fs_readsuper.data_size = data_size;

    fs_sendrec(fs_ep, &m);
    if (grant != GRANT_INVALID) mgrant_revoke(grant);

    int retval = m.u.m_fs_vfs_create_reply.status;

    if (!retval) {
        res->dev = dev;
        res->fs_ep = fs_ep;
        res->inode_nr = m.u.m_fs_vfs_create_reply.num;
        res->mode = m.u.m_fs_vfs_create_reply.mode;
        res->uid = m.u.m_fs_vfs_create_reply.uid;
        res->gid = m.u.m_fs_vfs_create_reply.gid;
        res->size = m.u.m_fs_vfs_create_reply.size;
    }

    return retval;
}
