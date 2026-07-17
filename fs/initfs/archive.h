#ifndef _INITFS_ARCHIVE_H_
#define _INITFS_ARCHIVE_H_

#include <sys/types.h>

#define INITFS_NAME_MAX 256

struct initfs_entry {
    char name[INITFS_NAME_MAX];
    char link[INITFS_NAME_MAX];
    off_t data_offset;
    off_t next_offset;
    size_t size;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    dev_t rdev;
};

struct initfs_format {
    const char* name;
    int (*probe)(dev_t dev);
    int (*read_entry)(dev_t dev, off_t offset, struct initfs_entry* entry);
};

extern const struct initfs_format initfs_tar_format;
extern const struct initfs_format initfs_cpio_format;

int initfs_read_bytes(dev_t dev, off_t offset, void* buf, size_t len);

#endif
