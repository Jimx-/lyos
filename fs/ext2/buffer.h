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

#ifndef _EXT2_BUFFER_H_
#define _EXT2_BUFFER_H_

struct ext2_buffer {
    struct list_head list;
    struct list_head hash;

    char *  b_data;
    dev_t   b_dev;
    block_t b_block;
    char    b_dirt;
    int     b_refcnt;
    size_t  b_size;
    int     b_flags;
};

typedef struct ext2_buffer ext2_buffer_t;

#endif

