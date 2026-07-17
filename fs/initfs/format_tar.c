#include <lyos/types.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "archive.h"
#include "tar.h"

static unsigned long tar_octal(const char* value, size_t len)
{
    unsigned long result = 0;

    while (len && (*value == ' ' || *value == '\0')) {
        value++;
        len--;
    }
    while (len-- && *value >= '0' && *value <= '7')
        result = (result << 3) + *value++ - '0';

    return result;
}

static mode_t tar_mode(const struct posix_tar_header* h)
{
    mode_t mode;

    switch (h->typeflag) {
    case REGTYPE:
    case AREGTYPE:
        mode = S_IFREG;
        break;
    case SYMTYPE:
        mode = S_IFLNK;
        break;
    case CHRTYPE:
        mode = S_IFCHR;
        break;
    case BLKTYPE:
        mode = S_IFBLK;
        break;
    case DIRTYPE:
        mode = S_IFDIR;
        break;
    case FIFOTYPE:
        mode = S_IFIFO;
        break;
    default:
        mode = 0;
        break;
    }
    return mode | tar_octal(h->mode, sizeof(h->mode));
}

static int tar_probe(dev_t dev)
{
    char magic[6];

    if (initfs_read_bytes(dev, 257, magic, sizeof(magic)) != 0) return 0;
    return !memcmp(magic, "ustar", 5);
}

static int tar_read_entry(dev_t dev, off_t offset, struct initfs_entry* entry)
{
    char buf[512];
    struct posix_tar_header* h = (struct posix_tar_header*)buf;
    size_t prefix_len, name_len;

    if (initfs_read_bytes(dev, offset, buf, sizeof(buf)) != 0) return EIO;
    if (!h->name[0]) return ENOENT;

    memset(entry, 0, sizeof(*entry));
    prefix_len = strnlen(h->prefix, sizeof(h->prefix));
    name_len = strnlen(h->name, sizeof(h->name));
    if (prefix_len) {
        if (prefix_len + 1 + name_len >= sizeof(entry->name))
            return ENAMETOOLONG;
        memcpy(entry->name, h->prefix, prefix_len);
        entry->name[prefix_len++] = '/';
    }
    if (prefix_len + name_len >= sizeof(entry->name)) return ENAMETOOLONG;
    memcpy(entry->name + prefix_len, h->name, name_len);
    entry->link[sizeof(entry->link) - 1] = '\0';
    strncpy(entry->link, h->linkname, sizeof(entry->link) - 1);

    entry->size = tar_octal(h->size, sizeof(h->size));
    entry->uid = tar_octal(h->uid, sizeof(h->uid));
    entry->gid = tar_octal(h->gid, sizeof(h->gid));
    entry->mode = tar_mode(h);
    entry->rdev = MAKE_DEV(tar_octal(h->devmajor, sizeof(h->devmajor)),
                           tar_octal(h->devminor, sizeof(h->devminor)));
    entry->data_offset = offset + 512;
    entry->next_offset = entry->data_offset + ((entry->size + 511) & ~511UL);
    return 0;
}

const struct initfs_format initfs_tar_format = {
    .name = "tar", .probe = tar_probe, .read_entry = tar_read_entry};
