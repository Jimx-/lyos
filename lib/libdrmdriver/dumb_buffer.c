#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <asm/ioctl.h>
#include <drm/drm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

int drm_mode_create_dumb(struct drm_device* dev,
                         struct drm_mode_create_dumb* args)
{
    if (!dev->driver->dumb_create) {
        return ENOSYS;
    }

    if (!args->width || !args->height || !args->bpp) return EINVAL;

    return dev->driver->dumb_create(dev, args);
}

int drm_mode_create_dumb_ioctl(struct drm_device* dev, endpoint_t endpoint,
                               void* data)
{
    return drm_mode_create_dumb(dev, data);
}

int drm_mode_mmap_dumb_ioctl(struct drm_device* dev, endpoint_t endpoint,
                             void* data)
{
    struct drm_mode_map_dumb* args = data;

    if (!dev->driver->dumb_create) {
        return ENOSYS;
    }

    return drm_gem_dumb_map_offset(dev, args->handle, &args->offset);
}
