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

#ifndef _VFS_GLOBAL_H_
#define _VFS_GLOBAL_H_

/* EXTERN is extern except for global.c */
#ifdef _VFS_GLOBAL_VARIABLE_HERE_
#undef EXTERN
#define EXTERN
#endif
    
#include "fproc.h"
#include "thread.h"

/* FS */
EXTERN  int                 ROOT_DEV;
EXTERN  struct file_desc    f_desc_table[NR_FILE_DESC];
EXTERN  spinlock_t          f_desc_table_lock;
EXTERN  struct inode        inode_table[NR_INODE];
#define FSBUF_SIZE          0x100000
EXTERN  u8                  _fsbuf[FSBUF_SIZE];
extern  u8 *                fsbuf;
EXTERN  MESSAGE             fs_msg;
EXTERN  struct inode *      root_inode;
EXTERN  int                 err_code;

extern struct list_head filesystem_table;
/* vfs mount table */
extern struct list_head vfs_mount_table;

/* how many times the root is mounted */
extern int have_root;

extern int nr_workers;

#define INODE_HASH_LOG2   7    
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)

/* inode hash table */
EXTERN struct list_head vfs_inode_table[INODE_HASH_SIZE];

EXTERN volatile int fs_sleeping;
EXTERN struct worker_thread workers[NR_WORKER_THREADS];

#endif 
