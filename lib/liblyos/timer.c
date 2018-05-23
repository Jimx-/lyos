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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <lyos/timer.h>

PRIVATE DEF_LIST(_timer_list);
PRIVATE DEF_LIST(_timer_list_expiring);
PRIVATE int expiring = 0;

PUBLIC void timer_add(struct list_head* list, struct timer_list* timer)
{
    struct timer_list* tp;
    struct timer_list* prev = NULL;

    if (list_empty(list)) {
        list_add(&timer->list, list);
        return;
    }

    list_for_each_entry(tp, list, list) {
        if (tp->expire_time >= timer->expire_time) break;
        prev = tp;
    }    

    if (prev == NULL) {
        list_add(&timer->list, list);
    } else {
        list_add(&timer->list, &prev->list);
    }
}

PUBLIC void timer_expire(struct list_head* list, clock_t timestamp)
{
    struct timer_list* tp, * n;
    list_for_each_entry_safe(tp, n, list, list) {
        if (tp->expire_time <= timestamp) {
            timer_remove(tp);
            (*tp->callback)(tp);
        }
    }    
}

PUBLIC void timer_remove(struct timer_list* timer)
{
    timer->expire_time = TIMER_UNSET;

    list_del(&timer->list);
}

PUBLIC void set_timer(struct timer_list* timer, clock_t ticks, timer_callback_t cb, int arg)
{
    clock_t uptime;

    if (get_ticks(&uptime, NULL) != 0) panic("can't get uptime\n");

    timer->expire_time = uptime + ticks;
    timer->callback = cb;
    timer->arg = arg;

    clock_t prev_timeout, next_timeout;
    if (list_empty(&_timer_list)) prev_timeout = TIMER_UNSET;
    else prev_timeout = list_entry(_timer_list.next, struct timer_list, list)->expire_time;
        
    if (expiring) list_add(&timer->list, &_timer_list_expiring);
    else timer_add(&_timer_list, timer);

    if (list_empty(&_timer_list)) next_timeout = TIMER_UNSET;
    else next_timeout = list_entry(_timer_list.next, struct timer_list, list)->expire_time;
    
    if (!expiring && (prev_timeout == TIMER_UNSET || prev_timeout > next_timeout)) {
        if (kernel_alarm(next_timeout, 1) != 0) panic("can't set timer\n");
    }
}

PUBLIC void expire_timer(clock_t timestamp)
{
    expiring = 1;
    timer_expire(&_timer_list, timestamp);
    expiring = 0;

    struct timer_list* tp, * n;
    list_for_each_entry_safe(tp, n, &_timer_list_expiring, list) {
        list_del(&tp->list);
        timer_add(&_timer_list, tp);
    }  

    clock_t next_timeout;
    if (list_empty(&_timer_list)) next_timeout = TIMER_UNSET;
    else next_timeout = list_entry(_timer_list.next, struct timer_list, list)->expire_time;
    
    if (next_timeout != TIMER_UNSET) {
        if (kernel_alarm(next_timeout, 1) != 0) panic("can't set timer\n");
    }
}
