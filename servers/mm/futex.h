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


#ifndef _MM_FUTEX_H_
#define _MM_FUTEX_H_

union futex_key {
    struct {
        unsigned long addr;
        struct mm_struct* mm;
        int offset;
    } private;
    struct {
        unsigned long word;
        void* ptr;
        int offset;
    } both;
};

struct futex_entry {
    struct list_head list;

    struct mmproc* mmp;
    union futex_key key;

    u32 bitset;
};

#endif
