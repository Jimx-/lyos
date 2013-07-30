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
#include "errno.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
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
PUBLIC int register_filesystem(MESSAGE * p)
{
    int name_len = p->NAME_LEN;
    char * fs_name = (char *)alloc_mem(name_len + 1);

    if (!fs_name) return ENOMEM;

    phys_copy(va2la(getpid(), fs_name), va2la(p->source, p->PATHNAME), name_len);

    fs_name[name_len] = '\0';

    add_filesystem(fs_name, p->source);
    printl("VFS: Registered %s filesystem(pid: %d)\n", fs_name, p->source);

    return 0;
}

PUBLIC int add_filesystem(char * name, endpoint_t fs_ep)
{
    struct file_system * pfs = (struct file_system *)alloc_mem(sizeof(struct file_system));

    if (!pfs) {
        return ENOMEM;
    }

    pfs->name = name;
    pfs->fs_ep = fs_ep;

    list_add(&(pfs->list), &filesystem_table);
    return 0;
}

PUBLIC endpoint_t get_filesystem_endpoint(char * name)
{
    struct file_system * pfs;
    list_for_each_entry(pfs, &filesystem_table, list) {
        if (strcmp(pfs->name, name) == 0) {
            return pfs->fs_ep;
        }
    }
    return -1;
}

PUBLIC int request_readsuper(endpoint_t fs_ep, dev_t dev,
        int readonly, int is_root, struct lookup_result * res)
{
    MESSAGE m;
    m.type = FS_READSUPER;
    m.REQ_FLAGS = 0;
    if (readonly) m.REQ_FLAGS |= RF_READONLY;
    if (is_root) m.REQ_FLAGS |= RF_ISROOT;

    m.REQ_DEV3 = dev;

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

