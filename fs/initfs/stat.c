#include <lyos/types.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "global.h"
#include "proto.h"

int initfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data)
{
    const struct initfs_entry* entry;
    struct stat sbuf;

    if (num >= initfs_entries_count) return EINVAL;
    entry = &initfs_entries[num];
    memset(&sbuf, 0, sizeof(sbuf));
    sbuf.st_dev = dev;
    sbuf.st_ino = num;
    sbuf.st_mode = num ? entry->mode : S_IFDIR | S_IRWXU;
    sbuf.st_nlink = 1;
    sbuf.st_uid = entry->uid;
    sbuf.st_gid = entry->gid;
    sbuf.st_rdev = entry->rdev;
    sbuf.st_size = entry->size;
    return fsdriver_copyout(data, 0, &sbuf, sizeof(sbuf));
}
