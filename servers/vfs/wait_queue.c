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
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "wait_queue.h"

int default_wake_function(struct wait_queue_entry* wq_entry, void* arg)
{
    struct worker_thread* worker = wq_entry->private;

    if (worker) {
        worker_wake(worker);

        wq_entry->private = NULL;
        return 1;
    }

    return 0;
}

int autoremove_wake_function(struct wait_queue_entry* wq_entry, void* arg)
{
    int retval = default_wake_function(wq_entry, arg);

    if (retval) {
        list_del(&wq_entry->entry);
    }

    return retval;
}

void waitqueue_wakeup_all(struct wait_queue_head* head, void* arg)
{
    struct wait_queue_entry *curr, *next;
    int retval;

    list_for_each_entry_safe(curr, next, &head->head, entry)
    {
        retval = curr->func(curr, arg);
        if (retval < 0) break;
    }
}
