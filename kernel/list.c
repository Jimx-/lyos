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
#include "lyos/list.h"

PUBLIC int list_empty(struct list_head * list)
{
    return (list->next == list);
}

PRIVATE inline void __list_add(struct list_head * new, struct list_head * pre, struct list_head * next)
{
    new->prev = pre;
    new->next = next;
    pre->next = new;
    next->prev = new;
}

PUBLIC inline void list_add(struct list_head * new, struct list_head * head)
{
    __list_add(new, head, head->next);
}

PUBLIC inline void list_del(struct list_head * node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}
