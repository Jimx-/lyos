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
#include <sys/stat.h>
#include <lyos/sysutils.h>
#include <sys/syslimits.h>
#include "libfsdriver/libfsdriver.h"

static int get_next_name(char** ptr, char** start, char* name, size_t namesize)
{
    char* p;
    int i;

    for (p = *ptr; *p == '/'; p++)
        ;

    *start = p;

    if (*p) {
        for (i = 0; *p && *p != '/' && i < namesize; p++, i++)
            name[i] = *p;

        if (i >= namesize) return ENAMETOOLONG;

        name[i] = 0;
    } else
        strlcpy(name, ".", namesize);

    *ptr = p;
    return 0;
}

static int access_dir(struct fsdriver_node* fn, struct vfs_ucred* ucred)
{
    mode_t mask;

    if (!S_ISDIR(fn->fn_mode)) return ENOTDIR;

    if (ucred->uid == SU_UID) return 0;

    if (ucred->uid == fn->fn_uid)
        mask = S_IXUSR;
    else if (ucred->gid == fn->fn_gid)
        mask = S_IXGRP;
    else {
        mask = S_IXOTH;
    }

    return (fn->fn_mode & mask) ? 0 : EACCES;
}

static int resolve_link(const struct fsdriver* fsd, dev_t dev, ino_t num,
                        char* pathname, int path_len, char* ptr,
                        endpoint_t user_endpt)
{
    struct fsdriver_data data;
    char path[PATH_MAX];
    ssize_t retval;

    data.granter = SELF;
    data.ptr = path;
    size_t size = sizeof(path) - 1;

    if (fsd->fs_rdlink == NULL) return ENOSYS;

    if ((retval = fsd->fs_rdlink(dev, num, &data, size, user_endpt)) < 0) {
        return -retval;
    } else {
        size = retval;
    }

    if (size + strlen(ptr) >= sizeof(path)) return ENAMETOOLONG;

    strlcpy(&path[size], ptr, sizeof(path) - size);

    strlcpy(pathname, path, path_len);

    return 0;
}

int fsdriver_lookup(const struct fsdriver* fsd, MESSAGE* m)
{
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_lookup.dev;
    ino_t start = m->u.m_vfs_fs_lookup.start;
    ino_t root = m->u.m_vfs_fs_lookup.root;
    int flags = m->u.m_vfs_fs_lookup.flags;
    size_t name_len = m->u.m_vfs_fs_lookup.name_len;
    size_t path_size = m->u.m_vfs_fs_lookup.path_size;
    mgrant_id_t path_grant = m->u.m_vfs_fs_lookup.path_grant;
    endpoint_t user_endpt = m->u.m_vfs_fs_lookup.user_endpt;
    struct fsdriver_node cur_node, next_node;
    int retval = 0, is_mountpoint;
    struct vfs_ucred ucred;
    char pathname[PATH_MAX];
    int symloop = 0;

    ucred.uid = m->u.m_vfs_fs_lookup.uid;
    ucred.gid = m->u.m_vfs_fs_lookup.gid;

    if ((retval = fsdriver_copy_name(src, path_grant, name_len, pathname,
                                     sizeof(pathname), FALSE)) != 0)
        return retval;

    if (fsd->fs_lookup == NULL) return ENOSYS;

    char *cp, *last;
    char component[NAME_MAX + 1];

    strlcpy(component, ".", sizeof(component));
    retval = fsd->fs_lookup(dev, start, component, &cur_node, &is_mountpoint);
    if (retval) return retval;

    /* scan */
    for (cp = last = pathname; *cp != '\0';) {
        if ((retval =
                 get_next_name(&cp, &last, component, sizeof(component))) != 0)
            break;

        if (is_mountpoint) {
            if (strcmp(component, "..")) {
                retval = EINVAL;
                break;
            }
        } else {
            if ((retval = access_dir(&cur_node, &ucred)) != 0) break;
        }

        if (strcmp(component, ".") == 0) continue;

        if (strcmp(component, "..") == 0) {
            if (cur_node.fn_num == root) continue;

            if (cur_node.fn_num == fsd->root_num) {
                cp = last;
                retval = ELEAVEMOUNT;

                break;
            }
        }

        if ((retval = fsd->fs_lookup(dev, cur_node.fn_num, component,
                                     &next_node, &is_mountpoint)) != 0)
            break;

        /* resolve symlink */
        if (S_ISLNK(next_node.fn_mode) &&
            (*cp || !(flags & LKF_SYMLINK_NOFOLLOW))) {
            if (++symloop < _POSIX_SYMLOOP_MAX) {
                retval = resolve_link(fsd, dev, next_node.fn_num, pathname,
                                      sizeof(pathname), cp, user_endpt);
            } else
                retval = ELOOP;

            if (fsd->fs_putinode) fsd->fs_putinode(dev, next_node.fn_num, 1);

            if (retval) break;

            cp = pathname;

            /* absolute symlink */
            if (*cp == '/') {
                retval = ESYMLINK;
                break;
            }

            continue;
        }

        if (fsd->fs_putinode) fsd->fs_putinode(dev, cur_node.fn_num, 1);
        cur_node = next_node;

        if (is_mountpoint) {
            retval = EENTERMOUNT;
            break;
        }
    }

    if (retval == EENTERMOUNT || retval == ELEAVEMOUNT || retval == ESYMLINK) {
        int retval2 = 0;

        if (symloop > 0) {
            name_len = strlen(pathname) + 1;

            if (name_len > path_size) return ENAMETOOLONG;

            retval2 = safecopy_to(src, path_grant, 0, pathname, name_len);
        }

        if (retval2 == 0) {
            m->u.m_fs_vfs_lookup_reply.offset = cp - pathname;
            if (retval == EENTERMOUNT) {
                m->u.m_fs_vfs_lookup_reply.num = cur_node.fn_num;
            }
        } else
            retval = retval2;
    }

    if (retval == 0) {
        m->u.m_fs_vfs_lookup_reply.num = cur_node.fn_num;
        m->u.m_fs_vfs_lookup_reply.node.uid = cur_node.fn_uid;
        m->u.m_fs_vfs_lookup_reply.node.gid = cur_node.fn_gid;
        m->u.m_fs_vfs_lookup_reply.node.size = cur_node.fn_size;
        m->u.m_fs_vfs_lookup_reply.node.mode = cur_node.fn_mode;
        m->u.m_fs_vfs_lookup_reply.node.spec_dev = cur_node.fn_device;
    } else {
        if (fsd->fs_putinode) fsd->fs_putinode(dev, cur_node.fn_num, 1);
    }

    return retval;
}
