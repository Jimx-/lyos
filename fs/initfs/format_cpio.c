#include <lyos/types.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "archive.h"

#define CPIO_NEWC_HEADER_SIZE 110

static unsigned long cpio_hex(const char* value, size_t len, int* ok)
{
    unsigned long result = 0;

    *ok = 1;
    while (len--) {
        unsigned int digit;
        if (*value >= '0' && *value <= '9')
            digit = *value - '0';
        else if (*value >= 'a' && *value <= 'f')
            digit = *value - 'a' + 10;
        else if (*value >= 'A' && *value <= 'F')
            digit = *value - 'A' + 10;
        else {
            *ok = 0;
            return 0;
        }
        result = (result << 4) | digit;
        value++;
    }
    return result;
}

static int cpio_probe(dev_t dev)
{
    char magic[6];

    if (initfs_read_bytes(dev, 0, magic, sizeof(magic)) != 0) return 0;
    return !memcmp(magic, "070701", 6) || !memcmp(magic, "070702", 6);
}

static int cpio_read_entry(dev_t dev, off_t offset, struct initfs_entry* entry)
{
    char h[CPIO_NEWC_HEADER_SIZE];
    unsigned long namesize, major, minor;
    off_t data_offset;
    int ok;

    if (initfs_read_bytes(dev, offset, h, sizeof(h)) != 0) return EIO;
    if (memcmp(h, "070701", 6) && memcmp(h, "070702", 6)) return EINVAL;

    memset(entry, 0, sizeof(*entry));
    entry->mode = cpio_hex(h + 14, 8, &ok);
    if (!ok) return EINVAL;
    entry->uid = cpio_hex(h + 22, 8, &ok);
    if (!ok) return EINVAL;
    entry->gid = cpio_hex(h + 30, 8, &ok);
    if (!ok) return EINVAL;
    entry->size = cpio_hex(h + 54, 8, &ok);
    if (!ok) return EINVAL;
    major = cpio_hex(h + 78, 8, &ok);
    if (!ok) return EINVAL;
    minor = cpio_hex(h + 86, 8, &ok);
    if (!ok) return EINVAL;
    namesize = cpio_hex(h + 94, 8, &ok);
    if (!ok || !namesize || namesize > sizeof(entry->name)) return EINVAL;

    if (initfs_read_bytes(dev, offset + sizeof(h), entry->name, namesize) != 0)
        return EIO;
    if (entry->name[namesize - 1] != '\0') return EINVAL;
    if (!strcmp(entry->name, "TRAILER!!!")) return ENOENT;

    entry->rdev = MAKE_DEV(major, minor);
    data_offset = (offset + sizeof(h) + namesize + 3) & ~3UL;
    entry->data_offset = data_offset;
    entry->next_offset = (data_offset + entry->size + 3) & ~3UL;

    if (S_ISLNK(entry->mode)) {
        size_t len = entry->size;
        if (len >= sizeof(entry->link)) return ENAMETOOLONG;
        if (initfs_read_bytes(dev, data_offset, entry->link, len) != 0)
            return EIO;
        entry->link[len] = '\0';
    }
    return 0;
}

const struct initfs_format initfs_cpio_format = {
    .name = "cpio-newc", .probe = cpio_probe, .read_entry = cpio_read_entry};
