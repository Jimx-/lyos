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

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "lyos/config.h"

typedef struct spinlock {
    volatile unsigned int lock;
} spinlock_t;

/* Single CPU */
#ifndef CONFIG_SMP

#define DEF_SPINLOCK(l)
#define spinlock_init(lock)
#define spinlock_lock(lock)
#define spinlock_unlock(lock)
#define spinlock_locked(lock) 0

#else

#define DEF_SPINLOCK(l) spinlock_t l = {0}
#define spinlock_init(l) \
    do {                 \
        (l)->lock = 0;   \
    } while (0)

void arch_spinlock_lock(volatile unsigned int* lock);
void arch_spinlock_unlock(volatile unsigned int* lock);

static inline void spinlock_lock(spinlock_t* lock)
{
    arch_spinlock_lock(&lock->lock);
}
static inline void spinlock_unlock(spinlock_t* lock)
{
    arch_spinlock_unlock(&lock->lock);
}

#define spinlock_locked(l) ((l)->lock != 0)
#endif /* CONFIG_SMP */

#endif /* _SPINLOCK_H_ */
