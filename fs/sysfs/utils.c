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
#include "errno.h"
#include "stdio.h"
#include <stdlib.h>
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include <regex.h>
#include "libsysfs/libsysfs.h"
#include "libmemfs/libmemfs.h"
#include "node.h"
#include "global.h"
#include "proto.h"

struct subscription {
    struct list_head list;
    struct list_head events;
    endpoint_t owner;
    regex_t regex;
    int type_mask;
};

struct subscription_event {
    struct list_head list;
    char key[PATH_MAX + 1];
    int flags;
    int event;
};

struct sub_check_cb_data {
    struct subscription* sub;
    int match_found;
};

static DEF_LIST(subscriptions);

static sysfs_dyn_attr_id_t alloc_dyn_attr_id()
{
    static sysfs_dyn_attr_id_t next_id = 1;
    return next_id++;
}

static struct subscription* lookup_sub(endpoint_t owner)
{
    struct subscription* sub;

    list_for_each_entry(sub, &subscriptions, list)
    {
        if (sub->owner == owner) return sub;
    }
    return NULL;
}

static int check_sub_match(const struct subscription* sub, const char* path)
{
    return regexec(&sub->regex, path, 0, NULL, 0) == 0;
}

static void free_sub(struct subscription* sub)
{
    list_del(&sub->list);
    regfree(&sub->regex);
    free(sub);
}

static struct subscription_event* alloc_sub_event(const char* path, int flags,
                                                  int event_type)
{
    struct subscription_event* event;

    event = malloc(sizeof(*event));
    if (!event) return NULL;

    memset(event, 0, sizeof(*event));
    strlcpy(event->key, path, sizeof(event->key));
    event->flags = flags;
    event->event = event_type;

    return event;
}

static void update_subscribers(const sysfs_node_t* node, const char* path,
                               int event_type)
{
    struct subscription* sub;
    struct subscription_event* event;

    list_for_each_entry(sub, &subscriptions, list)
    {
        if (!(NODE_TYPE(node) & sub->type_mask)) continue;
        if (!check_sub_match(sub, path)) continue;

        event = alloc_sub_event(path, node->flags, event_type);
        if (!event) break;

        list_add_tail(&event->list, &sub->events);
        send_recv(NOTIFY, sub->owner, NULL);
    }
}

int do_publish(MESSAGE* m)
{
    endpoint_t src = m->source;
    size_t len = m->u.m_sysfs_req.key_len;
    int flags = m->u.m_sysfs_req.flags;
    char name[PATH_MAX];
    int retval;

    if (len > PATH_MAX) return EINVAL;

    /* fetch the name */
    if ((retval =
             safecopy_from(src, m->u.m_sysfs_req.key_grant, 0, name, len)) != 0)
        return retval;

    sysfs_node_t* node = create_node(name, flags);
    if (!node) return errno;

    if (flags & SF_TYPE_U32) {
        node->u.u32v = m->u.m_sysfs_req.val.u32;
    } else if (flags & SF_TYPE_DYNAMIC) {
        node->u.dyn_attr->owner = src;
        node->u.dyn_attr->id = alloc_dyn_attr_id();
        m->u.m_sysfs_req.val.attr_id = node->u.dyn_attr->id;
    }

    update_subscribers(node, name, SF_EVENT_PUBLISH);

    return 0;
}

int do_publish_link(MESSAGE* m)
{
    endpoint_t src = m->source;
    size_t target_len, link_path_len;
    char target[PATH_MAX];
    char link_path[PATH_MAX];
    sysfs_node_t *target_node, *link_node;
    int retval;

    target_len = m->u.m_sysfs_req.target_len;
    link_path_len = m->u.m_sysfs_req.key_len;

    if (target_len > PATH_MAX || link_path_len > PATH_MAX) return EINVAL;

    if ((retval = safecopy_from(src, m->u.m_sysfs_req.target_grant, 0, target,
                                target_len)) != 0) {
        return retval;
    }

    if ((retval = safecopy_from(src, m->u.m_sysfs_req.key_grant, 0, link_path,
                                link_path_len)) != 0) {
        return retval;
    }

    target_node = lookup_node_by_name(target);
    if (!target_node) return errno;

    link_node = create_node(link_path, SF_TYPE_LINK | SF_PRIV_OVERWRITE);
    if (!link_node) return errno;

    link_node->link_target = target_node;

    return 0;
}

int do_retrieve(MESSAGE* m)
{
    endpoint_t src = m->source;
    size_t len = m->u.m_sysfs_req.key_len;
    int flags = m->u.m_sysfs_req.flags;
    char name[PATH_MAX];
    int retval;

    if (len > PATH_MAX) return EINVAL;

    /* fetch the name */
    if ((retval =
             safecopy_from(src, m->u.m_sysfs_req.key_grant, 0, name, len)) != 0)
        return retval;

    sysfs_node_t* node = lookup_node_by_name(name);
    if (!node) return ENOENT;

    if (node->flags & SF_PRIV_RETRIEVE) return EPERM;

    if (flags & SF_TYPE_U32) {
        if (!(node->flags & SF_TYPE_U32)) return EINVAL;
        m->u.m_sysfs_req.val.u32 = node->u.u32v;
    }

    return 0;
}

static int subscribe_initial_check(const char* path, sysfs_node_t* node,
                                   void* cb_data)
{
    struct sub_check_cb_data* sub_check = cb_data;
    struct subscription* sub = sub_check->sub;
    struct subscription_event* event;

    if (!check_sub_match(sub_check->sub, path)) return 0;

    event = alloc_sub_event(path, node->flags, SF_EVENT_PUBLISH);
    if (!event) return ENOMEM;

    list_add_tail(&event->list, &sub->events);
    sub_check->match_found = TRUE;

    return 0;
}

int do_subscribe(MESSAGE* m)
{
    endpoint_t src = m->source;
    mgrant_id_t grant = m->u.m_sysfs_req.key_grant;
    size_t len = m->u.m_sysfs_req.key_len;
    int flags = m->u.m_sysfs_req.flags;
    char regex[PATH_MAX + 2];
    struct subscription* sub;
    struct sub_check_cb_data cb_data;
    int retval;

    if (len > PATH_MAX) return EINVAL;

    regex[0] = '^';
    if ((retval = safecopy_from(src, grant, 0, regex + 1, len)) != 0)
        return retval;
    strcat(regex, "$");

    sub = lookup_sub(src);
    if (sub) {
        if (!(flags & SF_OVERWRITE)) return EEXIST;
        free_sub(sub);
    }

    sub = malloc(sizeof(*sub));
    if (!sub) return ENOMEM;

    memset(sub, 0, sizeof(*sub));

    INIT_LIST_HEAD(&sub->events);

    if ((retval = regcomp(&sub->regex, regex, REG_EXTENDED)) != 0) {
        free(sub);
        return EINVAL;
    }

    sub->owner = src;
    sub->type_mask = flags & SF_TYPE_MASK;
    if (!sub->type_mask) sub->type_mask = SF_TYPE_MASK;

    list_add(&sub->list, &subscriptions);

    if (flags & SF_CHECK_NOW) {
        cb_data.sub = sub;
        cb_data.match_found = FALSE;

        retval = traverse_node(&root_node, sub->type_mask,
                               subscribe_initial_check, &cb_data);

        if (retval) return retval;

        if (cb_data.match_found) send_recv(NOTIFY, src, NULL);
    }

    return 0;
}

int do_get_event(MESSAGE* m)
{
    endpoint_t src = m->source;
    struct subscription* sub;
    struct subscription_event* event;
    int retval;

    sub = lookup_sub(src);
    if (!sub) return ESRCH;

    if (list_empty(&sub->events)) return ENOENT;

    event = list_first_entry(&sub->events, struct subscription_event, list);
    list_del(&event->list);

    if ((retval = safecopy_to(src, m->u.m_sysfs_req.key_grant, 0, event->key,
                              strlen(event->key) + 1)) != 0)
        return retval;

    m->u.m_sysfs_req.flags = event->flags;
    m->u.m_sysfs_req.event = event->event;
    free(event);

    return 0;
}
