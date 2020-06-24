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

#ifndef _EXT2_GLOBAL_H_
#define _EXT2_GLOBAL_H_

/* EXTERN is extern except for global.c */
#ifdef _EXT2_GLOBAL_VARIABLE_HERE_
#undef EXTERN
#define EXTERN
#endif

EXTERN int err_code;

/* super block table */
extern struct list_head ext2_superblock_table;

/* buffer */
#define EXT2FSBUF_SIZE 1024
extern u8* ext2fsbuf;
EXTERN u8 _ext2fsbuf[EXT2FSBUF_SIZE];

#define EXT2_INODE_HASH_LOG2 7
#define EXT2_INODE_HASH_SIZE ((unsigned long)1 << EXT2_INODE_HASH_LOG2)
#define EXT2_INODE_HASH_MASK (((unsigned long)1 << EXT2_INODE_HASH_LOG2) - 1)

/* inode hash table */
#define EXT2_NR_INODES 512
EXTERN struct list_head ext2_inode_table[EXT2_INODE_HASH_SIZE];
EXTERN ext2_inode_t ext2_inode_buffer[EXT2_NR_INODES];
EXTERN struct list_head ext2_unused_inode_list;

/* buffer cache */
#define MAX_BUFFERS 8192
#define EXT2_BUFFER_HASH_SIZE 128
EXTERN struct list_head ext2_buffer_cache[EXT2_BUFFER_HASH_SIZE];
extern struct list_head ext2_buffer_freelist, *ext2_buffer_freelist_tail;

#endif
