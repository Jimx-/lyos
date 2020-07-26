#ifndef _DRM_DRM_DEVICE_H_
#define _DRM_DRM_DEVICE_H_

#include <drm/drm_mode_config.h>

struct drm_driver;

struct drm_minor {
    int index;
    device_id_t device_id;
};

struct drm_device {
    device_id_t device_id;
    struct drm_driver* driver;

    struct drm_minor primary;

    struct drm_mode_config mode_config;

    struct idr gem_object_idr;
};

int drm_device_init(struct drm_device* dev, struct drm_driver* drv,
                    device_id_t parent);

#endif
