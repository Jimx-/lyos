#include <lyos/types.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "archive.h"
#include "global.h"
#include "proto.h"

static void fill_node(ino_t num, struct fsdriver_node* fn)
{
    const struct initfs_entry* entry = &initfs_entries[num];

    fn->fn_num = num;
    fn->fn_uid = entry->uid;
    fn->fn_gid = entry->gid;
    fn->fn_size = entry->size;
    fn->fn_mode = num ? entry->mode : S_IFDIR | S_IRWXU;
    fn->fn_device = entry->rdev;
}

static const char* normalized_name(const char* name)
{
    while (*name == '/')
        name++;
    if (name[0] == '.' && name[1] == '/') name += 2;
    return name;
}

int initfs_lookup(dev_t dev, ino_t start, const char* name,
                  struct fsdriver_node* fn, int* is_mountpoint)
{
    char path[INITFS_NAME_MAX];
    const char* parent;
    size_t len;
    int i;

    *is_mountpoint = FALSE;
    if (start >= initfs_entries_count) return EINVAL;
    if (!strcmp(name, ".")) {
        fill_node(start, fn);
        return 0;
    }

    parent = start ? normalized_name(initfs_entries[start].name) : "";
    len = strlen(parent);
    if (len && parent[len - 1] == '/') len--;
    if (len + (len != 0) + strlen(name) >= sizeof(path)) return ENAMETOOLONG;
    memcpy(path, parent, len);
    if (len) path[len++] = '/';
    strcpy(path + len, name);

    for (i = 0; i < initfs_entries_count; i++) {
        const char* candidate = normalized_name(initfs_entries[i].name);
        size_t candidate_len = strlen(candidate);
        while (candidate_len && candidate[candidate_len - 1] == '/')
            candidate_len--;
        if (strlen(path) == candidate_len &&
            !memcmp(path, candidate, candidate_len)) {
            fill_node(i, fn);
            return 0;
        }
    }
    return ENOENT;
}
