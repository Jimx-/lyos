#ifndef _TMPFS_TYPES_H_
#define _TMPFS_TYPES_H_

#include <sys/types.h>
#include <lyos/list.h>
#include <lyos/avl.h>
#include <sys/syslimits.h>

struct tmpfs_superblock;

struct tmpfs_inode {
    struct list_head hash;
    struct list_head list;

    dev_t dev;
    ino_t num;
    struct tmpfs_superblock* sb;
    int count;
    int update;
    int link_count;

    dev_t spec_dev;
    uid_t uid;
    gid_t gid;
    size_t size;
    mode_t mode;
    u32 atime;
    u32 ctime;
    u32 mtime;

    struct list_head children;

    char** pages;
    size_t nr_pages;

    unsigned int mountpoint : 1;
};

struct tmpfs_dentry {
    struct list_head list;
    char name[NAME_MAX];
    struct tmpfs_inode* inode;
};

struct tmpfs_superblock {
    struct list_head list;

    dev_t dev;
    struct tmpfs_inode* root;

    unsigned int readonly : 1;
};

#endif
