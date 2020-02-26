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

#ifndef _VFS_RWLOCK_H_
#define _VFS_RWLOCK_H_

#include "thread.h"

typedef enum { RWL_NONE, RWL_READ, RWL_WRITE } rwlock_type_t;

typedef struct {
    int state;
    int owner;

    pthread_mutex_t pending_mutex;
    unsigned int pending_reader_count;
    unsigned int pending_writer_count;
    /*struct list_head pending_writers;
    struct list_head pending_readers;*/

    unsigned int pending_reader_serial;
    unsigned int pending_writer_serial;
} rwlock_t;

PUBLIC void rwlock_init(rwlock_t* lock);
PUBLIC int rwlock_lock(rwlock_t* lock, rwlock_type_t type);
PUBLIC int rwlock_unlock(rwlock_t* lock);

#endif
