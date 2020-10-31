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

#ifndef _LIBFSDRIVER_H_
#define _LIBFSDRIVER_H_

#include <lyos/mgrant.h>
#include <libfsdriver/buffer.h>

struct fsdriver_node {
    dev_t fn_device;
    ino_t fn_num;
    uid_t fn_uid;
    gid_t fn_gid;
    size_t fn_size;
    mode_t fn_mode;
};

struct fsdriver_data {
    endpoint_t granter;
    union {
        mgrant_id_t grant;
        char* ptr;
    };
};

struct fsdriver_dentry_list {
    struct fsdriver_data* data;
    size_t data_size;
    off_t data_offset;
    char* buf;
    size_t buf_size;
    off_t buf_offset;
};

struct fsdriver {
    char* name;
    ino_t root_num;

    int (*fs_readsuper)(dev_t dev, int flags, struct fsdriver_node* node);
    int (*fs_mountpoint)(dev_t dev, ino_t num);
    int (*fs_putinode)(dev_t dev, ino_t num);
    int (*fs_lookup)(dev_t dev, ino_t start, const char* name,
                     struct fsdriver_node* fn, int* is_mountpoint);
    int (*fs_create)(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                     uid_t uid, gid_t gid, struct fsdriver_node* fn);
    int (*fs_mkdir)(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                    uid_t uid, gid_t gid);
    int (*fs_mknod)(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                    uid_t uid, gid_t gid, dev_t sdev);
    ssize_t (*fs_read)(dev_t dev, ino_t num, struct fsdriver_data* data,
                       loff_t rwpos, size_t count);
    ssize_t (*fs_write)(dev_t dev, ino_t num, struct fsdriver_data* data,
                        loff_t rwpos, size_t count);
    ssize_t (*fs_rdlink)(dev_t dev, ino_t num, struct fsdriver_data* data,
                         size_t bytes, endpoint_t user_endpt);
    int (*fs_symlink)(dev_t dev, ino_t dir_num, const char* name, uid_t uid,
                      gid_t gid, struct fsdriver_data* data, size_t bytes);
    int (*fs_unlink)(dev_t dev, ino_t dir_num, const char* name);
    int (*fs_rmdir)(dev_t dev, ino_t dir_num, const char* name);
    int (*fs_stat)(dev_t dev, ino_t num, struct fsdriver_data* data);
    int (*fs_ftrunc)(dev_t dev, ino_t num, off_t start_pos, off_t end_pos);
    int (*fs_chmod)(dev_t dev, ino_t num, mode_t* mode);
    ssize_t (*fs_getdents)(dev_t dev, ino_t num, struct fsdriver_data* data,
                           loff_t* position, size_t count);
    int (*fs_utime)(dev_t dev, ino_t num, const struct timespec* atime,
                    const struct timespec* mtime);
    int (*fs_driver)(dev_t dev);
    int (*fs_sync)();

    void (*fs_other)(MESSAGE* m);
} __attribute__((packed));

int fsdriver_start(const struct fsdriver* fsd);

int fsdriver_copyin(struct fsdriver_data* data, size_t offset, void* buf,
                    size_t len);
int fsdriver_copyout(struct fsdriver_data* data, size_t offset, void* buf,
                     size_t len);
int fsdriver_copy_name(endpoint_t endpoint, mgrant_id_t grant, size_t len,
                       char* name, size_t size, int non_empty);

int fsdriver_dentry_list_init(struct fsdriver_dentry_list* list,
                              struct fsdriver_data* data, size_t data_size,
                              char* buf, size_t buf_size);
int fsdriver_dentry_list_add(struct fsdriver_dentry_list* list, ino_t num,
                             const char* name, size_t name_len, int type);
int fsdriver_dentry_list_finish(struct fsdriver_dentry_list* list);

int fsdriver_driver(dev_t dev);

#endif
