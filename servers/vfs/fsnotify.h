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

#ifndef _VFS_FSNOTIFY_H_
#define _VFS_FSNOTIFY_H_

#include <lyos/idr.h>
#include <lyos/list.h>

#include "wait_queue.h"

#define FS_ACCESS        0x00000001 /* File was accessed */
#define FS_MODIFY        0x00000002 /* File was modified */
#define FS_ATTRIB        0x00000004 /* Metadata changed */
#define FS_CLOSE_WRITE   0x00000008 /* Writtable file was closed */
#define FS_CLOSE_NOWRITE 0x00000010 /* Unwrittable file closed */
#define FS_OPEN          0x00000020 /* File was opened */
#define FS_MOVED_FROM    0x00000040 /* File was moved from X */
#define FS_MOVED_TO      0x00000080 /* File was moved to Y */
#define FS_CREATE        0x00000100 /* Subfile was created */
#define FS_DELETE        0x00000200 /* Subfile was deleted */
#define FS_DELETE_SELF   0x00000400 /* Self was deleted */
#define FS_MOVE_SELF     0x00000800 /* Self was moved */
#define FS_OPEN_EXEC     0x00001000 /* File was opened for exec */

#define FS_UNMOUNT    0x00002000 /* inode on umount fs */
#define FS_Q_OVERFLOW 0x00004000 /* Event queued overflowed */
#define FS_IN_IGNORED 0x00008000 /* last inotify event here */

#define FS_OPEN_PERM   0x00010000 /* open event in an permission hook */
#define FS_ACCESS_PERM 0x00020000 /* access event in a permissions hook */
#define FS_OPEN_EXEC_PERM                                                \
    0x00040000                   /* open/exec event in a permission hook \
                                  */
#define FS_DIR_MODIFY 0x00080000 /* Directory entry was modified */

#define FS_EXCL_UNLINK                                     \
    0x04000000 /* do not send events if object is unlinked \
                */
/* This inode cares about things that happen to its children.  Always set for
 * dnotify and inotify. */
#define FS_EVENT_ON_CHILD 0x08000000

#define FS_DN_RENAME    0x10000000 /* file renamed */
#define FS_DN_MULTISHOT 0x20000000 /* dnotify multishot */
#define FS_ISDIR        0x40000000 /* event occurred against dir */
#define FS_IN_ONESHOT   0x80000000 /* only send event once */

#define FS_MOVE (FS_MOVED_FROM | FS_MOVED_TO)

#define ALL_FSNOTIFY_DIRENT_EVENTS \
    (FS_CREATE | FS_DELETE | FS_MOVE | FS_DIR_MODIFY)

#define ALL_FSNOTIFY_PERM_EVENTS \
    (FS_OPEN_PERM | FS_ACCESS_PERM | FS_OPEN_EXEC_PERM)

#define FS_EVENTS_POSS_ON_CHILD                                     \
    (ALL_FSNOTIFY_PERM_EVENTS | FS_ACCESS | FS_MODIFY | FS_ATTRIB | \
     FS_CLOSE_WRITE | FS_CLOSE_NOWRITE | FS_OPEN | FS_OPEN_EXEC)

#define ALL_FSNOTIFY_EVENTS                                                  \
    (ALL_FSNOTIFY_DIRENT_EVENTS | FS_EVENTS_POSS_ON_CHILD | FS_DELETE_SELF | \
     FS_MOVE_SELF | FS_DN_RENAME | FS_UNMOUNT | FS_Q_OVERFLOW | FS_IN_IGNORED)

struct fsnotify_group;
struct fsnotify_mark;
struct fsnotify_mark_connector;

struct fsnotify_event {
    struct list_head list;
};

struct fsnotify_ops {
    int (*handle_event)(struct fsnotify_group* group, struct inode* pin,
                        u32 mask, const struct inode* data,
                        const char* filename, u32 cookie,
                        struct fsnotify_mark* mark);
    void (*freeing_mark)(struct fsnotify_mark* mark,
                         struct fsnotify_group* group);
    void (*free_mark)(struct fsnotify_mark* mark);
    void (*free_event)(struct fsnotify_event* event);
};

struct fsnotify_group {
    const struct fsnotify_ops* ops;

    unsigned int refcnt;

    struct list_head notifications;
    struct wait_queue_head wqh;

    unsigned int num_marks;
    struct list_head mark_list;

    unsigned int q_len;
    unsigned int max_events;

    union {
        struct {
            struct idr idr;
        } inotify_data;
    };
};

struct fsnotify_mark {
    __u32 mask;
    int refcnt;

    struct list_head group_link;
    struct list_head obj_link;

    struct fsnotify_group* group;
    struct fsnotify_mark_connector* connector;

#define FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY 0x01
#define FSNOTIFY_MARK_FLAG_ALIVE               0x02
#define FSNOTIFY_MARK_FLAG_ATTACHED            0x04
    int flags;
};

enum fsnotify_obj_type {
    FSNOTIFY_OBJ_TYPE_INODE,
    FSNOTIFY_OBJ_TYPE_COUNT,
    FSNOTIFY_OBJ_TYPE_DETACHED = FSNOTIFY_OBJ_TYPE_COUNT
};

struct fsnotify_mark_connector {
    unsigned short type;
    struct fsnotify_mark_connector** obj;
    struct list_head marks;
};

static inline int fsnotify_valid_obj_type(unsigned int type)
{
    return (type < FSNOTIFY_OBJ_TYPE_COUNT);
}

static inline struct inode*
fsnotify_conn_inode(struct fsnotify_mark_connector* conn)
{
    return list_entry(conn->obj, struct inode, i_fsnotify_marks);
}

static inline int fsnotify_notify_queue_empty(struct fsnotify_group* group)
{
    return list_empty(&group->notifications);
}
int fsnotify_alloc_group(const struct fsnotify_ops* ops,
                         struct fsnotify_group** gp);
void fsnotify_destroy_group(struct fsnotify_group* group);

static inline void fsnotify_get_group(struct fsnotify_group* group)
{
    group->refcnt++;
}
void fsnotify_put_group(struct fsnotify_group* group);

void fsnotify_init_mark(struct fsnotify_mark* mark,
                        struct fsnotify_group* group);
void fsnotify_destroy_mark(struct fsnotify_mark* mark);

static inline void fsnotify_get_mark(struct fsnotify_mark* mark)
{
    mark->refcnt++;
}
void fsnotify_put_mark(struct fsnotify_mark* mark);

int fsnotify_add_mark(struct fsnotify_mark* mark,
                      struct fsnotify_mark_connector** connp, unsigned int type,
                      int allow_dups);

static inline int fsnotify_add_inode_mark(struct fsnotify_mark* mark,
                                          struct inode* pin, int allow_dups)
{
    return fsnotify_add_mark(mark, &pin->i_fsnotify_marks,
                             FSNOTIFY_OBJ_TYPE_INODE, allow_dups);
}

static inline void fsnotify_init_event(struct fsnotify_event* event)
{
    INIT_LIST_HEAD(&event->list);
}
void fsnotify_destroy_event(struct fsnotify_group* group,
                            struct fsnotify_event* event);
int fsnotify_add_event(struct fsnotify_group* group,
                       struct fsnotify_event* event);

#endif /* _VFS_FSNOTIFY_H_ */
