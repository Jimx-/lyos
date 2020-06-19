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

#ifndef _ARCH_ATOMIC_H_
#define _ARCH_ATOMIC_H_

typedef struct {
    volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) \
    {                  \
        (i)            \
    }
#define INIT_ATOMIC(v, i) \
    do {                  \
        (v)->counter = i; \
    } while (0)

static inline int atomic_get(atomic_t* v) { return v->counter; }

static inline void atomic_inc(atomic_t* v)
{
    asm volatile("lock incl %0" : "+m"(v->counter));
}

static inline void atomic_dec(atomic_t* v)
{
    asm volatile("lock decl %0" : "+m"(v->counter));
}

static inline int atomic_dec_and_test(atomic_t* v)
{
    unsigned char c;

    asm volatile("lock decl %0; sete %1"
                 : "+m"(v->counter), "=qm"(c)
                 :
                 : "memory");
    return c != 0;
}

#endif
