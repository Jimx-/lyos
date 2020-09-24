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

#ifndef _VFS_TYPES_H_
#define _VFS_TYPES_H_

#include <lyos/spinlock.h>
#include <lyos/types.h>
#include "thread.h"
#include "wait_queue.h"

struct inode;
struct file_desc;
struct poll_table;

struct file_operations {
    ssize_t (*read)(struct file_desc*, char*, size_t, loff_t*, struct fproc*);
    ssize_t (*write)(struct file_desc*, const char*, size_t, loff_t*,
                     struct fproc*);
    int (*ioctl)(struct inode*, struct file_desc*, unsigned int, unsigned long,
                 struct fproc*);
    __poll_t (*poll)(struct file_desc*, __poll_t, struct poll_table*,
                     struct fproc*);
    int (*open)(struct inode*, struct file_desc*);
    int (*release)(struct inode*, struct file_desc*);
};

/**
 * @struct inode
 * @brief  i-node
 *
 * The \c start_sect and\c nr_sects locate the file in the device,
 * and the size show how many bytes is used.
 * If <tt> size < (nr_sects * SECTOR_SIZE) </tt>, the rest bytes
 * are wasted and reserved for later writing.
 *
 * \b NOTE: Remember to change INODE_SIZE if the members are changed
 */
struct inode {
    struct list_head list;
    endpoint_t i_fs_ep; /**< FS process's pid */
    u32 i_mode;         /**< Accsess mode */
    u32 i_size;         /**< File size */
    u32 i_start_sect;   /**< The first sector of the data */
    u32 i_nr_sects;     /**< How many sectors the file occupies */
    uid_t i_uid;        /* uid and gid */
    gid_t i_gid;
    dev_t i_dev;     /**< On which device this inode resides */
    dev_t i_specdev; /**< Device number for block/character special file */
    int i_cnt;       /**< How many procs share this inode  */
    int i_num;       /**< inode nr.  */
    rwlock_t i_lock;

    struct vfs_mount* i_vmnt;
    const struct file_operations* i_fops;

    void* i_private;
};

struct vfs_mount {
    struct list_head list;
    int m_fs_ep;                /**< FS process's pid */
    int m_dev;                  /**< Device number */
    unsigned int m_flags;       /**< Mount flags */
    struct inode* m_mounted_on; /**< Mount point */
    struct inode* m_root_node;  /**< Root inode */
    char m_label[FS_LABEL_MAX]; /**< Label of the file system process */
    rwlock_t m_lock;
};

/**
 * @def   MAX_FILENAME_LEN
 * @brief Max len of a filename
 * @see   dir_entry
 */
#define MAX_FILENAME_LEN 12

/**
 * @struct file_desc
 * @brief  File Descriptor
 */
struct file_desc {
    int fd_mode;            /**< R or W */
    int fd_pos;             /**< Current position for R/W. */
    int fd_cnt;             /**< How many procs share this desc */
    struct inode* fd_inode; /**< Ptr to the i-node */
    const struct file_operations* fd_fops;
    void* fd_private_data;
    mutex_t fd_lock;
};

/* character device mapping */
struct cdmap {
    endpoint_t driver;
    struct wait_queue_head wait;
};

struct file_system {
    struct list_head list;
    char name[FS_LABEL_MAX];
    int fs_ep;
};

#endif
