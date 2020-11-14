#ifndef _DRM_DRM_DEVICE_H_
#define _DRM_DRM_DEVICE_H_

#include <lyos/list.h>

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

    endpoint_t incaller;
    int inid;
    mgrant_id_t ingrant;
    size_t inleft;
    size_t incnt;

    __poll_t select_ops;

    int event_space;
    struct list_head pending_event_list;
    struct list_head event_list;
};

int drm_device_init(struct drm_device* dev, struct drm_driver* drv,
                    device_id_t parent);

#endif
