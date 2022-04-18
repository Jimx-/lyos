#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <asm/ioctl.h>
#include <drm/drm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <asm/page.h>

#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

struct drm_gem_object* drm_gem_object_lookup(struct drm_device* dev, u32 handle)
{
    return idr_find(&dev->gem_object_idr, handle);
}

void drm_gem_open(struct drm_device* dev)
{
    idr_init_base(&dev->gem_object_idr, 1);
}

int drm_gem_handle_create(struct drm_device* dev, struct drm_gem_object* obj,
                          unsigned int* handlep)
{
    unsigned int handle;
    int retval;

    retval = idr_alloc(&dev->gem_object_idr, obj, 1, 0);

    if (retval < 0) return -retval;

    handle = retval;
    *handlep = handle;

    return 0;
}

int drm_gem_object_init(struct drm_gem_object* obj, size_t size)
{
    obj->size = size;

    return 0;
}

int drm_gem_dumb_map_offset(struct drm_device* dev, u32 handle, u64* offset)
{
    struct drm_gem_object* obj;

    obj = drm_gem_object_lookup(dev, handle);
    if (!obj) return ENOENT;

    *offset = handle << ARCH_PG_SHIFT;

    return 0;
}

static int drm_gem_mmap_obj(struct drm_device* dev, struct drm_gem_object* obj,
                            endpoint_t endpoint, char* addr, size_t length,
                            char** retaddr)
{
    if (obj->size < length) {
        return EINVAL;
    }

    if (obj->funcs && obj->funcs->mmap) {
        return obj->funcs->mmap(obj, endpoint, addr, length, retaddr);
    }

    return EINVAL;
}

int drm_gem_mmap(struct drm_device* dev, endpoint_t endpoint, char* addr,
                 off_t offset, size_t length, char** retaddr)
{
    struct drm_gem_object* obj;
    unsigned int handle = offset >> ARCH_PG_SHIFT;

    obj = drm_gem_object_lookup(dev, handle);
    if (!obj) return EINVAL;

    return drm_gem_mmap_obj(dev, obj, endpoint, addr, length, retaddr);
}
