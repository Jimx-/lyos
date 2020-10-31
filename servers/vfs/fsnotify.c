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

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

#include "fsnotify.h"

static inline u32* fsnotify_conn_mask_p(struct fsnotify_mark_connector* conn)
{
    switch (conn->type) {
    case FSNOTIFY_OBJ_TYPE_INODE:
        return &fsnotify_conn_inode(conn)->i_fsnotify_mask;
    default:
        break;
    }

    return NULL;
}

int fsnotify_alloc_group(const struct fsnotify_ops* ops,
                         struct fsnotify_group** gp)
{
    struct fsnotify_group* group;

    group = malloc(sizeof(*group));
    if (!group) return -ENOMEM;

    memset(group, 0, sizeof(*group));

    group->refcnt = 1;
    init_waitqueue_head(&group->wqh);
    group->max_events = UINT_MAX;
    INIT_LIST_HEAD(&group->notifications);
    INIT_LIST_HEAD(&group->mark_list);

    group->ops = ops;

    *gp = group;
    return 0;
}

static void fsnotify_destroy_group_final(struct fsnotify_group* group)
{
    free(group);
}

void fsnotify_put_group(struct fsnotify_group* group)
{
    assert(group->refcnt > 0);
    if (--group->refcnt == 0) fsnotify_destroy_group_final(group);
}

void fsnotify_init_mark(struct fsnotify_mark* mark,
                        struct fsnotify_group* group)
{
    memset(mark, 0, sizeof(*mark));
    mark->refcnt = 1;
    fsnotify_get_group(group);
    mark->group = group;
    INIT_LIST_HEAD(&mark->group_link);
    INIT_LIST_HEAD(&mark->obj_link);
}

static struct fsnotify_mark_connector*
fsnotify_get_connector(struct fsnotify_mark_connector** connp)
{
    struct fsnotify_mark_connector* conn = *connp;

    if (conn && conn->type == FSNOTIFY_OBJ_TYPE_DETACHED) return NULL;

    return conn;
}

static int fsnotify_attach_connector(struct fsnotify_mark_connector** connp,
                                     unsigned int type)
{
    struct fsnotify_mark_connector* conn;

    conn = malloc(sizeof(*conn));
    if (!conn) return -ENOMEM;

    INIT_LIST_HEAD(&conn->marks);
    conn->type = type;
    conn->obj = connp;

    if (type == FSNOTIFY_OBJ_TYPE_INODE) dup_inode(fsnotify_conn_inode(conn));

    *connp = conn;

    return 0;
}

static void* fsnotify_detach_connector(struct fsnotify_mark_connector* conn,
                                       unsigned int* type)
{
    struct inode* pin = NULL;

    *type = conn->type;
    if (conn->type == FSNOTIFY_OBJ_TYPE_DETACHED) return NULL;

    if (conn->type == FSNOTIFY_OBJ_TYPE_INODE) {
        pin = fsnotify_conn_inode(conn);
        pin->i_fsnotify_mask = 0;
    }

    *conn->obj = NULL;
    conn->obj = NULL;
    conn->type = FSNOTIFY_OBJ_TYPE_DETACHED;

    return pin;
}

static int fsnotify_add_mark_list(struct fsnotify_mark* mark,
                                  struct fsnotify_mark_connector** connp,
                                  unsigned int type, int allow_dups)
{
    struct fsnotify_mark_connector* conn;
    struct fsnotify_mark* lmark;
    int retval;

    conn = fsnotify_get_connector(connp);
    if (!conn) {
        retval = fsnotify_attach_connector(connp, type);
        if (retval) return retval;
        conn = *connp;
    }

    assert(conn);

    if (!allow_dups) list_for_each_entry(lmark, &conn->marks, obj_link)
        {
            if (lmark->group == mark->group &&
                (lmark->flags & FSNOTIFY_MARK_FLAG_ATTACHED))
                return -EEXIST;
        }

    list_add(&mark->obj_link, &conn->marks);
    mark->connector = conn;

    return 0;
}

static void fsnotify_recalc_mask(struct fsnotify_mark_connector* conn)
{
    u32 new_mask = 0;
    struct fsnotify_mark* mark;

    if (!conn) return;

    if (!fsnotify_valid_obj_type(conn->type)) return;

    list_for_each_entry(mark, &conn->marks, obj_link)
    {
        if (mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED) new_mask |= mark->mask;
    }

    *fsnotify_conn_mask_p(conn) = new_mask;
}

int fsnotify_add_mark(struct fsnotify_mark* mark,
                      struct fsnotify_mark_connector** connp, unsigned int type,
                      int allow_dups)
{
    int retval;

    struct fsnotify_group* group = mark->group;

    mark->flags |= FSNOTIFY_MARK_FLAG_ALIVE | FSNOTIFY_MARK_FLAG_ATTACHED;

    list_add(&mark->group_link, &group->mark_list);
    fsnotify_get_mark(mark);
    group->num_marks++;

    retval = fsnotify_add_mark_list(mark, connp, type, allow_dups);
    if (retval) goto err;

    if (mark->mask) fsnotify_recalc_mask(mark->connector);

    return retval;

err:
    mark->flags &= ~(FSNOTIFY_MARK_FLAG_ALIVE | FSNOTIFY_MARK_FLAG_ATTACHED);
    list_del(&mark->group_link);
    group->num_marks--;
    fsnotify_put_mark(mark);

    return retval;
}

void fsnotify_detach_mark(struct fsnotify_mark* mark)
{
    struct fsnotify_group* group = mark->group;

    if (!(mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED)) return;

    mark->flags &= ~FSNOTIFY_MARK_FLAG_ATTACHED;
    list_del(&mark->group_link);
    group->num_marks--;

    fsnotify_put_mark(mark);
}

void fsnotify_free_mark(struct fsnotify_mark* mark)
{
    struct fsnotify_group* group = mark->group;

    if (!(mark->flags & FSNOTIFY_MARK_FLAG_ALIVE)) return;
    mark->flags &= ~FSNOTIFY_MARK_FLAG_ALIVE;

    if (group->ops->freeing_mark) group->ops->freeing_mark(mark, group);
}

void fsnotify_destroy_mark(struct fsnotify_mark* mark)
{
    fsnotify_detach_mark(mark);
    fsnotify_free_mark(mark);
}

static void fsnotify_destroy_mark_final(struct fsnotify_mark* mark)
{
    struct fsnotify_group* group = mark->group;

    if (!group) return;

    group->ops->free_mark(mark);
    fsnotify_put_group(group);
}

static void fsnotify_drop_object(unsigned int type, void* objp)
{
    struct inode* pin;

    if (!objp) return;
    if (type != FSNOTIFY_OBJ_TYPE_INODE) return;

    pin = objp;
    put_inode(pin);
}

void fsnotify_put_mark(struct fsnotify_mark* mark)
{
    struct fsnotify_mark_connector* conn = mark->connector;
    void* objp = NULL;
    int free_conn = FALSE;
    unsigned int type;

    assert(mark->refcnt > 0);

    if (!conn) {
        if (--mark->refcnt == 0) fsnotify_destroy_mark_final(mark);
        return;
    }

    if (--mark->refcnt > 0) return;

    list_del(&mark->obj_link);
    if (list_empty(&conn->marks)) {
        objp = fsnotify_detach_connector(conn, &type);
        free_conn = TRUE;
    } else
        fsnotify_recalc_mask(conn);

    mark->connector = NULL;

    fsnotify_drop_object(type, objp);

    if (free_conn) free(conn);

    fsnotify_destroy_mark_final(mark);
}

static void fsnotify_clear_marks(struct fsnotify_group* group)
{
    struct fsnotify_mark *mark, *tmp;

    list_for_each_entry_safe(mark, tmp, &group->mark_list, group_link)
    {
        fsnotify_get_mark(mark);
        fsnotify_detach_mark(mark);
        fsnotify_free_mark(mark);
        fsnotify_put_mark(mark);
    }
}

void fsnotify_destroy_group(struct fsnotify_group* group)
{
    struct fsnotify_event* event;

    fsnotify_clear_marks(group);

    while (!fsnotify_notify_queue_empty(group)) {
        event = list_first_entry(&group->notifications, struct fsnotify_event,
                                 list);
        fsnotify_destroy_event(group, event);
    }

    fsnotify_put_group(group);
}

void fsnotify_destroy_event(struct fsnotify_group* group,
                            struct fsnotify_event* event)
{
    if (!event) return;

    group->ops->free_event(event);
}

int fsnotify_add_event(struct fsnotify_group* group,
                       struct fsnotify_event* event)
{
    group->q_len++;
    list_add_tail(&event->list, &group->notifications);

    waitqueue_wakeup_all(&group->wqh, NULL);
    return 0;
}

static int send_to_group(struct inode* pin, u32 mask, const struct inode* data,
                         u32 cookie, const char* filename,
                         struct fsnotify_mark* mark)
{
    struct fsnotify_group* group;
    u32 test_mask = (mask & ALL_FSNOTIFY_EVENTS);
    u32 mark_mask = 0;

    group = mark->group;
    mark_mask = mark->mask;

    if (!(test_mask & mark_mask)) return 0;

    return group->ops->handle_event(group, pin, mask, data, filename, cookie,
                                    mark);
}

int fsnotify(struct inode* pin, u32 mask, const struct inode* data,
             const char* filename, u32 cookie)
{
    struct fsnotify_mark* mark;
    int retval;

    if (!pin->i_fsnotify_mask) return 0;

    list_for_each_entry(mark, &pin->i_fsnotify_marks->marks, obj_link)
    {
        retval = send_to_group(pin, mask, data, cookie, filename, mark);

        if (retval && (mask & ALL_FSNOTIFY_PERM_EVENTS)) return retval;
    }

    return 0;
}

void fsnotify_create(struct inode* dir, struct inode* child, const char* name)
{
    fsnotify(dir, FS_CREATE, child, name, 0);
}

void fsnotify_unlink(struct inode* dir, struct inode* child, const char* name)
{
    fsnotify(dir, FS_DELETE, child, name, 0);
}
