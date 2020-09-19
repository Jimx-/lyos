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

#ifndef _TIMER_H_
#define _TIMER_H_

#include <lyos/list.h>

struct timer_list;

typedef void (*timer_callback_t)(struct timer_list* timer);

struct timer_list {
    struct list_head list;
    clock_t expire_time;
    timer_callback_t callback;
    void* arg;
};

#define TIMER_UNSET ((clock_t)0xffffffff)

void timer_remove(struct timer_list* timer);
void set_timer(struct timer_list* timer, clock_t ticks, timer_callback_t cb,
               void* arg);
void cancel_timer(struct timer_list* timer);
void expire_timer(clock_t timestamp);

#endif
