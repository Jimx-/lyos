#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>

#include <libmemfs/libmemfs.h>
#include "proto.h"

static ssize_t get_target_path(struct memfs_inode* parent,
                               struct memfs_inode* target, char* path);

ssize_t memfs_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                     size_t bytes, endpoint_t user_endpt)
{
    char path[PATH_MAX];
    struct memfs_inode *pin, *target;
    int retval;
    ssize_t len;

    if (!fs_hooks.rdlink_hook) {
        return -ENOSYS;
    }

    pin = memfs_find_inode(num);
    if (!pin) {
        return -EINVAL;
    }

    retval = fs_hooks.rdlink_hook(pin, pin->data, &target, user_endpt);
    if (retval) return retval;
    assert(target != NULL);

    len = get_target_path(pin->i_parent, target, path);
    if (len < 0) return len;

    assert(len > 0 && len < sizeof(path));

    if (len > bytes) {
        len = bytes;
    }

    retval = fsdriver_copyout(data, 0, path, len);
    if (retval) {
        return retval;
    }

    return len;
}

static ssize_t get_target_path(struct memfs_inode* parent,
                               struct memfs_inode* target, char* path)
{
    struct memfs_inode *base, *pin;
    char* p = path;
    size_t len = 0, plen;

    base = parent;
    if (base) {
        while (base->i_parent) {
            pin = target->i_parent;

            if (pin) {
                while (pin->i_parent && base != pin) {
                    pin = pin->i_parent;
                }
            }

            if (base == pin) {
                break;
            }

            if ((p - path + 3) >= PATH_MAX) {
                return -ENAMETOOLONG;
            }

            strcpy(p, "../");
            p += 3;
            base = base->i_parent;
        }
    }

    pin = target;
    while (pin->i_parent && pin != base) {
        len += strlen(pin->i_name) + 1;
        pin = pin->i_parent;
    }

    if (len < 2) {
        return -EINVAL;
    }
    len--;

    plen = (p - path) + len;
    if (plen >= PATH_MAX) {
        return -ENAMETOOLONG;
    }

    pin = target;
    while (pin->i_parent && pin != base) {
        size_t slen = strlen(pin->i_name);
        len -= slen;

        memcpy(p + len, pin->i_name, slen);
        if (len) p[--len] = '/';

        pin = pin->i_parent;
    }

    return plen;
}

int memfs_mkdir(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid)
{
    struct memfs_inode* pin;
    struct memfs_stat stat;

    if ((pin = memfs_find_inode(dir_num)) == NULL) return EINVAL;

    if (memfs_find_inode_by_name(pin, name) != NULL) return EEXIST;

    if (!fs_hooks.mknod_hook) return ENOSYS;

    memset(&stat, 0, sizeof(stat));
    stat.st_dev = dev;
    stat.st_mode = mode;
    stat.st_uid = uid;
    stat.st_gid = gid;
    stat.st_device = NO_DEV;

    return fs_hooks.mkdir_hook(pin, name, &stat, pin->data);
}

int memfs_mknod(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid, dev_t sdev)
{
    struct memfs_inode* pin;
    struct memfs_stat stat;

    if ((pin = memfs_find_inode(dir_num)) == NULL) return EINVAL;

    if (memfs_find_inode_by_name(pin, name) != NULL) return EEXIST;

    if (!fs_hooks.mknod_hook) return ENOSYS;

    memset(&stat, 0, sizeof(stat));
    stat.st_dev = dev;
    stat.st_mode = mode;
    stat.st_uid = uid;
    stat.st_gid = gid;
    stat.st_device = sdev;

    return fs_hooks.mknod_hook(pin, name, &stat, pin->data);
}
