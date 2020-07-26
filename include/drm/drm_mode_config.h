#ifndef _DRM_DRM_MODE_CONFIG_H_
#define _DRM_DRM_MODE_CONFIG_H_

#include <sys/types.h>
#include <lyos/list.h>
#include <lyos/idr.h>

struct drm_device;
struct drm_mode_fb_cmd2;
struct drm_framebuffer;

struct drm_mode_config_funcs {
    int (*fb_create)(struct drm_device* dev, const struct drm_mode_fb_cmd2* cmd,
                     struct drm_framebuffer** fbp);
};

struct drm_mode_config {
    int min_width, min_height;
    int max_width, max_height;
    const struct drm_mode_config_funcs* funcs;

    size_t num_fb;
    struct list_head fb_list;

    size_t num_crtc;
    struct list_head crtc_list;

    size_t num_connector;
    struct list_head connector_list;

    size_t num_encoder;
    struct list_head encoder_list;

    size_t num_total_plane;
    struct list_head plane_list;

    struct idr object_idr;
};

void drm_mode_config_init(struct drm_device* drm_device);

#endif
