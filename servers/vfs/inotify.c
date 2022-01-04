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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

#include "fsnotify.h"

#define ALL_INOTIFY_BITS                                                       \
    (IN_ACCESS | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE |   \
     IN_OPEN | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE |           \
     IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED | \
     IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK | IN_MASK_ADD |              \
     IN_MASK_CREATE | IN_ISDIR | IN_ONESHOT)

struct inotify_event_info {
    struct fsnotify_event fse;
    u32 mask;
    int wd;
    u32 cookie;
    int name_len;
    char name[];
};

struct inotify_inode_mark {
    struct fsnotify_mark fsn_mark;
    int wd;
};

static unsigned int inotify_max_queued_events = 10;

static int inotify_handle_event(struct fsnotify_group* group, struct inode* pin,
                                u32 mask, const struct inode* data,
                                const char* filename, u32 cookie,
                                struct fsnotify_mark* mark);
static void inotify_freeing_mark(struct fsnotify_mark* mark,
                                 struct fsnotify_group* group);
static void inotify_free_mark(struct fsnotify_mark* mark);
static void inotify_free_event(struct fsnotify_event* mark);

static const struct fsnotify_ops inotify_fsnotify_ops = {
    .handle_event = inotify_handle_event,
    .freeing_mark = inotify_freeing_mark,
    .free_mark = inotify_free_mark,
    .free_event = inotify_free_event,
};

static inline struct inotify_event_info*
to_i_event(struct fsnotify_event* fsn_event)
{
    return list_entry(fsn_event, struct inotify_event_info, fse);
}

static inline u32 inotify_arg_to_mask(u32 arg)
{
    u32 mask;

    mask = (FS_IN_IGNORED | FS_EVENT_ON_CHILD | FS_UNMOUNT);
    mask |= (arg & (IN_ALL_EVENTS | IN_ONESHOT | IN_EXCL_UNLINK));

    return mask;
}

static inline u32 inotify_mask_to_arg(u32 mask)
{
    return mask &
           (IN_ALL_EVENTS | IN_ISDIR | IN_UNMOUNT | IN_IGNORED | IN_Q_OVERFLOW);
}

static int inotify_create_group(unsigned int max_events,
                                struct fsnotify_group** gp)
{
    struct fsnotify_group* group;
    int retval;

    retval = fsnotify_alloc_group(&inotify_fsnotify_ops, &group);
    if (retval) return retval;

    group->max_events = max_events;

    idr_init(&group->inotify_data.idr);

    *gp = group;
    return 0;
}

static int inotify_add_idr(struct idr* idr, struct inotify_inode_mark* mark)
{
    int retval;

    retval = idr_alloc(idr, mark, 1, 0);
    if (retval >= 0) {
        mark->wd = retval;
        fsnotify_get_mark(&mark->fsn_mark);
    }

    return retval < 0 ? retval : 0;
}

static struct inotify_inode_mark* inotify_find_idr(struct fsnotify_group* group,
                                                   int wd)
{
    struct inotify_inode_mark* mark;

    mark = idr_find(&group->inotify_data.idr, wd);
    if (mark) {
        fsnotify_get_mark(&mark->fsn_mark);
        assert(mark->fsn_mark.refcnt >= 2);
    }

    return mark;
}

static void inotify_remove_idr(struct fsnotify_group* group,
                               struct inotify_inode_mark* mark)
{
    struct inotify_inode_mark* found_mark;
    int wd;

    wd = mark->wd;
    assert(wd != -1);

    found_mark = inotify_find_idr(group, wd);
    assert(found_mark == mark);
    assert(mark->fsn_mark.mask >= 2);

    idr_remove(&group->inotify_data.idr, wd);

    fsnotify_put_mark(&found_mark->fsn_mark);
    mark->wd = -1;
    fsnotify_put_mark(&mark->fsn_mark);
}

static int inotify_create_watch(struct fsnotify_group* group, struct inode* pin,
                                u32 arg)
{
    struct inotify_inode_mark* mark;
    u32 mask;
    struct idr* idr = &group->inotify_data.idr;
    int retval;

    mask = inotify_arg_to_mask(arg);

    mark = malloc(sizeof(*mark));
    if (!mark) return -ENOMEM;

    fsnotify_init_mark(&mark->fsn_mark, group);
    mark->fsn_mark.mask = mask;
    mark->wd = -1;

    retval = inotify_add_idr(idr, mark);
    if (retval) goto out;

    retval =
        fsnotify_add_inode_mark(&mark->fsn_mark, pin, FALSE /* allow_dups */);
    if (retval) {
        inotify_remove_idr(group, mark);
        goto out;
    }

    retval = mark->wd;

out:
    fsnotify_put_mark(&mark->fsn_mark);
    return retval;
}

static inline int inotify_update_watch(struct fsnotify_group* group,
                                       struct inode* pin, u32 arg)
{
    return inotify_create_watch(group, pin, arg);
}

static int inotify_handle_event(struct fsnotify_group* group, struct inode* pin,
                                u32 mask, const struct inode* data,
                                const char* filename, u32 cookie,
                                struct fsnotify_mark* mark)
{
    struct inotify_inode_mark* i_mark;
    struct inotify_event_info* event;
    struct fsnotify_event* fsn_event;
    int alloc_len = sizeof(struct inotify_event_info);
    int len = 0;
    int retval;

    if (filename) {
        len = strlen(filename);
        alloc_len += len + 1;
    }

    i_mark = list_entry(mark, struct inotify_inode_mark, fsn_mark);

    event = malloc(alloc_len);
    if (!event) return -ENOMEM;

    if (mask & (IN_MOVE_SELF | IN_DELETE_SELF)) mask &= ~IN_ISDIR;

    fsn_event = &event->fse;
    fsnotify_init_event(fsn_event);
    event->mask = mask;
    event->wd = i_mark->wd;
    event->cookie = cookie;
    event->name_len = len;
    if (len) strlcpy(event->name, filename, len + 1);

    retval = fsnotify_add_event(group, fsn_event);
    if (retval) fsnotify_destroy_event(group, fsn_event);

    if (mark->mask & IN_ONESHOT) fsnotify_destroy_mark(mark);

    return 0;
}

static void inotify_freeing_mark(struct fsnotify_mark* mark,
                                 struct fsnotify_group* group)
{
    struct inotify_inode_mark* i_mark =
        list_entry(mark, struct inotify_inode_mark, fsn_mark);
    inotify_remove_idr(group, i_mark);
}

static void inotify_free_mark(struct fsnotify_mark* mark)
{
    struct inotify_inode_mark* i_mark =
        list_entry(mark, struct inotify_inode_mark, fsn_mark);
    free(i_mark);
}

static void inotify_free_event(struct fsnotify_event* event)
{
    free(to_i_event(event));
}

static int get_inotify_fd_flags(int inflags)
{
    int flags = 0;

    if (inflags & IN_CLOEXEC) flags |= O_CLOEXEC;
    if (inflags & IN_NONBLOCK) flags |= O_NONBLOCK;

    return flags;
}

static int get_one_event(struct fsnotify_group* group, size_t count,
                         struct fsnotify_event** eventp)
{
    size_t event_size = sizeof(struct inotify_event);
    struct fsnotify_event* event;
    struct inotify_event_info* i_event;

    *eventp = NULL;

    if (fsnotify_notify_queue_empty(group)) return 0;

    event =
        list_first_entry(&group->notifications, struct fsnotify_event, list);
    i_event = to_i_event(event);

    if (i_event->name_len)
        event_size +=
            roundup(i_event->name_len + 1, sizeof(struct inotify_event));
    if (event_size > count) return -EINVAL;

    list_del(&event->list);

    *eventp = event;
    return 0;
}

static ssize_t copy_event_to_user(struct fsnotify_group* group,
                                  struct fsnotify_event* fsn_event,
                                  endpoint_t user_endpt, char* buf)
{
    struct inotify_event_info* event;
    struct inotify_event inotify_event;
    size_t event_size = sizeof(struct inotify_event);
    size_t name_len, name_len_pad;
    int retval;

    event = to_i_event(fsn_event);
    name_len = event->name_len;

    name_len_pad = roundup(name_len + 1, sizeof(struct inotify_event));
    inotify_event.wd = event->wd;
    inotify_event.len = name_len_pad;
    inotify_event.mask = inotify_mask_to_arg(event->mask);
    inotify_event.cookie = event->cookie;

    if ((retval = data_copy(user_endpt, buf, SELF, &inotify_event,
                            event_size)) != OK)
        return -retval;

    buf += event_size;

    if (name_len_pad) {
        if ((retval = data_copy(user_endpt, buf, SELF, event->name,
                                name_len + 1)) != OK)
            return -retval;

        event_size += name_len_pad;
    }

    return event_size;
}

static ssize_t inotify_read(struct file_desc* filp, char* buf, size_t count,
                            loff_t* ppos, struct fproc* fp)
{
    struct fsnotify_group* group;
    struct fsnotify_event* fsn_event;
    char* start;
    DECLARE_WAITQUEUE(wait, self);
    int retval;

    start = buf;
    group = filp->fd_private_data;

    waitqueue_add(&group->wqh, &wait);
    while (TRUE) {
        retval = get_one_event(group, count, &fsn_event);
        if (retval) break;

        if (fsn_event) {
            retval = copy_event_to_user(group, fsn_event, fp->endpoint, buf);
            fsnotify_destroy_event(group, fsn_event);
            if (retval < 0) break;

            buf += retval;
            count -= retval;
            continue;
        }

        retval = -EAGAIN;
        if (filp->fd_flags & O_NONBLOCK) break;

        if (start != buf) break;

        worker_wait(WT_BLOCKED_ON_INOTIFY);
    }
    waitqueue_remove(&group->wqh, &wait);

    if (start != buf && retval != -EFAULT) retval = buf - start;
    return retval;
}

static __poll_t inotify_poll(struct file_desc* filp, __poll_t mask,
                             struct poll_table* wait, struct fproc* fp)
{
    struct fsnotify_group* group = filp->fd_private_data;

    mask = 0;
    poll_wait(filp, &group->wqh, wait);

    if (!fsnotify_notify_queue_empty(group)) mask |= EPOLLIN | EPOLLRDNORM;

    return mask;
}

static int inotify_release(struct inode* pin, struct file_desc* filp)
{
    struct fsnotify_group* group = filp->fd_private_data;

    fsnotify_destroy_group(group);
    return 0;
}

static const struct file_operations inotify_fops = {
    .read = inotify_read,
    .poll = inotify_poll,
    .release = inotify_release,
};

int do_inotify_init1(void)
{
    int flags = self->msg_in.u.m_vfs_inotify.flags;
    int filp_flags = get_inotify_fd_flags(flags);
    struct fsnotify_group* group;
    int retval;

    if (flags & ~(IN_CLOEXEC | IN_NONBLOCK)) return -EINVAL;

    retval = inotify_create_group(inotify_max_queued_events, &group);
    if (retval != OK) return retval;

    retval =
        anon_inode_get_fd(fproc, 0, &inotify_fops, group,
                          O_RDONLY | (filp_flags & (O_NONBLOCK | O_CLOEXEC)));
    if (retval < 0) fsnotify_destroy_group(group);

    return retval;
}

int do_inotify_add_watch(void)
{
    int fd = self->msg_in.u.m_vfs_inotify.fd;
    int name_len = self->msg_in.u.m_vfs_inotify.name_len;
    u32 mask = self->msg_in.u.m_vfs_inotify.mask;
    struct file_desc* filp;
    char pathname[PATH_MAX + 1];
    struct lookup lookup;
    struct inode* pin;
    struct vfs_mount* vmnt;
    int lookup_flags = 0;
    struct fsnotify_group* group;
    int retval;

    if (mask & ~ALL_INOTIFY_BITS) return -EINVAL;
    if (!(mask & ALL_INOTIFY_BITS)) return -EINVAL;

    if (name_len > PATH_MAX) return -ENAMETOOLONG;

    if ((retval = data_copy(SELF, pathname, fproc->endpoint,
                            self->msg_in.u.m_vfs_inotify.pathname, name_len)) !=
        OK)
        return -retval;
    pathname[name_len] = 0;

    filp = get_filp(fproc, fd, RWL_WRITE);
    if (!filp) return -EBADF;

    if ((mask & IN_MASK_ADD) && (mask & IN_MASK_CREATE)) {
        retval = -EINVAL;
        goto out_unlock_filp;
    }

    if (filp->fd_fops != &inotify_fops) {
        retval = -EINVAL;
        goto out_unlock_filp;
    }

    if (mask & IN_DONT_FOLLOW) lookup_flags |= LKF_SYMLINK_NOFOLLOW;

    init_lookup(&lookup, pathname, lookup_flags, &vmnt, &pin);
    lookup.vmnt_lock = RWL_WRITE;
    lookup.inode_lock = RWL_WRITE;
    pin = resolve_path(&lookup, fproc);

    if (!pin) {
        retval = -err_code;
        goto out_unlock_filp;
    }

    group = filp->fd_private_data;

    retval = inotify_update_watch(group, pin, mask);

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

out_unlock_filp:
    unlock_filp(filp);

    return retval;
}
