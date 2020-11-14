#ifndef _DRM_DRM_VBLANK_H_
#define _DRM_DRM_VBLANK_H_

struct drm_pending_vblank_event {
    struct drm_pending_event base;

    unsigned int pipe;

    union {
        struct drm_event base;
        struct drm_event_vblank vbl;
    } event;
};

#endif
