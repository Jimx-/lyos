#ifndef _LIBDRMDRIVER_H_
#define _LIBDRMDRIVER_H_

#include <sys/types.h>
#include <stdint.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_device.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_gem.h>

#include <libdevman/libdevman.h>

struct drm_driver {
    const char* name;
    const char* desc;
    const char* date;

    int major;
    int minor;
    int patchlevel;

    int (*dumb_create)(struct drm_device* dev,
                       struct drm_mode_create_dumb* args);
};

int drmdriver_register_device(struct drm_device* dev);
int drmdriver_task(struct drm_device* drm_dev);

#endif
