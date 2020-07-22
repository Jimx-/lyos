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
#include "global.h"

int drm_mode_create_dumb(struct drm_mode_create_dumb* args)
{
    struct drm_driver* drv = drm_driver_tab;

    if (!drv->dumb_create) {
        return ENOSYS;
    }

    if (!args->width || !args->height || !args->bpp) return EINVAL;

    return drv->dumb_create(args);
}

int drm_mode_create_dumb_ioctl(endpoint_t endpoint, void* data)
{
    return drm_mode_create_dumb(data);
}

int drm_mode_mmap_dumb_ioctl(endpoint_t endpoint, void* data)
{
    struct drm_mode_map_dumb* args = data;
    struct drm_driver* drv = drm_driver_tab;

    if (!drv->dumb_create) {
        return ENOSYS;
    }

    return drm_gem_dumb_map_offset(args->handle, &args->offset);
}
