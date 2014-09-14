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

#ifndef _SYSFS_NODE_H_
#define _SYSFS_NODE_H_

typedef struct {
    struct list_head list;
    struct list_head children;

    char name[SF_ENTRY_LABEL_MAX];
    int type;
    int flags;

    union {
        char * str;
        u32 u32v;
        dev_t devno;
    } u;
} sysfs_node_t;

#endif /* _SYSFS_NODE_H_ */
