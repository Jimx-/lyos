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

#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

int drm_mode_object_add(struct drm_device* dev, struct drm_mode_object* obj,
                        unsigned int type)
{
    int retval;

    retval = idr_alloc(&dev->mode_config.object_idr, obj, 1, 0);
    if (retval >= 0) {
        obj->id = retval;
        obj->type = type;
    }

    return retval < 0 ? -retval : 0;
}

struct drm_mode_object* drm_mode_object_find(struct drm_device* dev,
                                             unsigned int id, unsigned type)
{
    struct drm_mode_object* obj = NULL;

    obj = idr_find(&dev->mode_config.object_idr, id);

    if (obj && type != DRM_MODE_OBJECT_ANY && obj->type != type) obj = NULL;
    if (obj && obj->id != id) obj = NULL;

    return obj;
}
