#ifndef _DRM_DRM_FILE_H_
#define _DRM_DRM_FILE_H_

#include <lyos/list.h>

struct drm_device;

struct drm_pending_event {
    struct drm_event* event;

    struct drm_device* dev;

    struct list_head link;
    struct list_head pending_link;
};

#endif
