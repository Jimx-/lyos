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
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/ipc.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sys/futex.h>

#include "rwlock.h"

PUBLIC void rwlock_init(rwlock_t* lock)
{
    pthread_mutex_init(&lock->lock, NULL);
}

PUBLIC int rwlock_lock(rwlock_t* lock, rwlock_type_t type)
{
    if (type != RWL_NONE)
        pthread_mutex_lock(&lock->lock);
    return 0;
}

PUBLIC int rwlock_unlock(rwlock_t* lock)
{
    pthread_mutex_unlock(&lock->lock);
    return 0;
}
