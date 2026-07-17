#include <lyos/types.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <lyos/sysutils.h>

#include "archive.h"
#include "const.h"
#include "global.h"
#include "proto.h"

static const struct initfs_format* const formats[] = {
    &initfs_tar_format,
    &initfs_cpio_format,
};

int initfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                     struct fsdriver_node* node)
{
    off_t offset = 0;
    unsigned int i;
    int retval;

    initfs_format = NULL;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        if (formats[i]->probe(dev)) {
            initfs_format = formats[i];
            break;
        }
    }
    if (!initfs_format) return EINVAL;

    initfs_entries_count = 0;
    while (initfs_entries_count < MAX_HEADERS) {
        struct initfs_entry* entry = &initfs_entries[initfs_entries_count];

        retval = initfs_format->read_entry(dev, offset, entry);
        if (retval == ENOENT) break;
        if (retval != 0) return retval;
        offset = entry->next_offset;
        initfs_entries_count++;
    }
    if (!initfs_entries_count || initfs_entries_count == MAX_HEADERS)
        return EINVAL;

    printl("initfs: detected %s archive with %d entries\n", initfs_format->name,
           initfs_entries_count);

    node->fn_num = 0;
    node->fn_uid = 0;
    node->fn_gid = 0;
    node->fn_size = 0;
    node->fn_mode = S_IFDIR | S_IRWXU;
    return 0;
}
