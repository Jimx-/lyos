#ifndef _DRM_DRM_PLANE_H_
#define _DRM_DRM_PLANE_H_

#include <sys/types.h>
#include <lyos/list.h>
#include <drm/drm_mode_object.h>

struct drm_device;

enum drm_plane_type {
    DRM_PLANE_TYPE_OVERLAY,
    DRM_PLANE_TYPE_PRIMARY,
    DRM_PLANE_TYPE_CURSOR,
};

struct drm_plane {
    struct list_head head;
    struct drm_mode_object base;
    unsigned index;

    enum drm_plane_type type;
    uint32_t possible_crtcs;
};

int drm_plane_init(struct drm_device* dev, struct drm_plane* plane,
                   uint32_t possible_crtcs, enum drm_plane_type type);

#endif
