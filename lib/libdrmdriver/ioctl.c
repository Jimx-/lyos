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
#include <lyos/mgrant.h>

#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "libdrmdriver.h"
#include "proto.h"

typedef int drm_ioctl_t(struct drm_device* dev, endpoint_t endpoint,
                        void* data);

struct drm_ioctl_desc {
    unsigned int cmd;
    int flags;
    drm_ioctl_t* func;
    const char* name;
};

#define DRM_IOCTL_DEF(ioctl, _func, _flags) \
    [_IOC_NR(ioctl)] = {                    \
        .cmd = ioctl, .func = _func, .flags = _flags, .name = #ioctl}

static int drm_getcap(struct drm_device* dev, endpoint_t endpoint, void* data)
{
    struct drm_get_cap* req = data;

    req->value = 0;

    switch (req->capability) {
    case DRM_CAP_DUMB_BUFFER:
        if (dev->driver->dumb_create) {
            req->value = 1;
        }
        break;
    default:
        return EINVAL;
    }

    return 0;
}

static const struct drm_ioctl_desc drm_ioctls[] = {
    DRM_IOCTL_DEF(DRM_IOCTL_GET_CAP, drm_getcap, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETRESOURCES, drm_mode_getresources, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETCRTC, drm_mode_getcrtc, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_SETCRTC, drm_mode_setcrtc, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETENCODER, drm_mode_getencoder, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETCONNECTOR, drm_mode_getconnector, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_ADDFB, drm_mode_addfb_ioctl, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_PAGE_FLIP, drm_mode_page_flip_ioctl, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_CREATE_DUMB, drm_mode_create_dumb_ioctl, 0),
    DRM_IOCTL_DEF(DRM_IOCTL_MODE_MAP_DUMB, drm_mode_mmap_dumb_ioctl, 0),
};

#define DRM_CORE_IOCTL_COUNT (sizeof(drm_ioctls) / sizeof(drm_ioctls[0]))

int drm_do_ioctl(struct drm_device* dev, int request, endpoint_t endpoint,
                 mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id)
{
    unsigned int nr = _IOC_NR(request);
    const struct drm_ioctl_desc* ioctl = NULL;
    char stack_data[128];
    char* data = NULL;
    size_t in_size, out_size, drv_size, ksize;
    int retval = EINVAL;

    if (nr >= DRM_CORE_IOCTL_COUNT) {
        goto err;
    }

    ioctl = &drm_ioctls[nr];
    if (!ioctl || !ioctl->func) {
        goto err;
    }

    drv_size = _IOC_SIZE(ioctl->cmd);
    out_size = in_size = _IOC_SIZE(request);
    if ((request & ioctl->cmd & IOC_IN) == 0) in_size = 0;
    if ((request & ioctl->cmd & IOC_OUT) == 0) out_size = 0;
    ksize = max(max(in_size, out_size), drv_size);

    if (ksize <= sizeof(stack_data)) {
        data = stack_data;
    } else {
        data = malloc(ksize);
        if (!data) {
            retval = ENOMEM;
            goto err;
        }
    }

    if (in_size > 0) {
        retval = safecopy_from(endpoint, grant, 0, data, in_size);
        if (retval) {
            goto err;
        }
    }

    if (ksize > in_size) memset(data + in_size, 0, ksize - in_size);

    retval = ioctl->func(dev, user_endpoint, data);
    if (retval) goto err;

    if (out_size > 0) {
        retval = safecopy_to(endpoint, grant, 0, data, out_size);
    }

err:
    if (data != NULL && data != stack_data) {
        free(data);
    }

    return retval;
}
