#ifndef _LIBDRMDRIVER_H_
#define _LIBDRMDRIVER_H_

#include <sys/types.h>
#include <stdint.h>
#include <lyos/list.h>
#include <lyos/idr.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <libdevman/libdevman.h>

#define DRM_MODE_OBJECT_CRTC      0xcccccccc
#define DRM_MODE_OBJECT_CONNECTOR 0xc0c0c0c0
#define DRM_MODE_OBJECT_ENCODER   0xe0e0e0e0
#define DRM_MODE_OBJECT_MODE      0xdededede
#define DRM_MODE_OBJECT_PROPERTY  0xb0b0b0b0
#define DRM_MODE_OBJECT_FB        0xfbfbfbfb
#define DRM_MODE_OBJECT_BLOB      0xbbbbbbbb
#define DRM_MODE_OBJECT_PLANE     0xeeeeeeee
#define DRM_MODE_OBJECT_ANY       0

struct drm_mode_object {
    unsigned int id;
    unsigned int type;
};

struct drm_crtc;

struct drm_encoder {
    struct list_head head;
    struct drm_mode_object base;
    unsigned index;

    int encoder_type;
    int encoder_type_id;

    uint32_t possible_crtcs;
    uint32_t possible_clones;

    struct drm_crtc* crtc;
};

struct drm_connector {
    struct list_head head;
    struct drm_mode_object base;
    unsigned index;

    int connector_type;
    int connector_type_id;

    uint32_t possible_encoders;
    struct drm_encoder* encoder;

    struct list_head modes;
};

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

struct drm_framebuffer {
    struct list_head head;
    struct drm_mode_object base;

    struct drm_gem_object* obj[4];
};

struct drm_mode_set;
struct drm_crtc_funcs {
    int (*set_config)(struct drm_mode_set* set);
};

struct drm_crtc {
    struct list_head head;
    struct drm_mode_object base;
    unsigned index;
    const struct drm_crtc_funcs* funcs;

    struct drm_plane* primary;
    struct drm_plane* cursor;
};

struct drm_mode_config_funcs {
    int (*fb_create)(const struct drm_mode_fb_cmd2* cmd,
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

enum drm_mode_status {
    MODE_OK = 0,
    MODE_HSYNC,
    MODE_VSYNC,
    MODE_H_ILLEGAL,
    MODE_V_ILLEGAL,
    MODE_BAD_WIDTH,
    MODE_NOMODE,
    MODE_NO_INTERLACE,
    MODE_NO_DBLESCAN,
    MODE_NO_VSCAN,
    MODE_MEM,
    MODE_VIRTUAL_X,
    MODE_VIRTUAL_Y,
    MODE_MEM_VIRT,
    MODE_NOCLOCK,
    MODE_CLOCK_HIGH,
    MODE_CLOCK_LOW,
    MODE_CLOCK_RANGE,
    MODE_BAD_HVALUE,
    MODE_BAD_VVALUE,
    MODE_BAD_VSCAN,
    MODE_HSYNC_NARROW,
    MODE_HSYNC_WIDE,
    MODE_HBLANK_NARROW,
    MODE_HBLANK_WIDE,
    MODE_VSYNC_NARROW,
    MODE_VSYNC_WIDE,
    MODE_VBLANK_NARROW,
    MODE_VBLANK_WIDE,
    MODE_PANEL,
    MODE_INTERLACE_WIDTH,
    MODE_ONE_WIDTH,
    MODE_ONE_HEIGHT,
    MODE_ONE_SIZE,
    MODE_NO_REDUCED,
    MODE_NO_STEREO,
    MODE_NO_420,
    MODE_STALE = -3,
    MODE_BAD = -2,
    MODE_ERROR = -1
};

struct drm_display_mode {
    struct list_head head;

    char name[DRM_DISPLAY_MODE_LEN];
    enum drm_mode_status status;
    unsigned int type;

    int clock; /* in kHz */
    int hdisplay;
    int hsync_start;
    int hsync_end;
    int htotal;
    int hskew;
    int vdisplay;
    int vsync_start;
    int vsync_end;
    int vtotal;
    int vscan;
    unsigned int flags;

    int width_mm;
    int height_mm;

    int vrefresh;
};

struct drm_mode_set {
    struct drm_framebuffer* fb;
    struct drm_crtc* crtc;
    struct drm_display_mode* mode;

    uint32_t x;
    uint32_t y;

    struct drm_connector** connectors;
    size_t num_connectors;
};

struct drm_gem_object;
struct drm_gem_object_funcs {
    int (*mmap)(struct drm_gem_object* obj, endpoint_t endpoint, char* addr,
                size_t length, char** retaddr);
};

struct drm_gem_object {
    size_t size;

    const struct drm_gem_object_funcs* funcs;
};

struct drm_driver {
    const char* name;
    device_id_t device_id;
    struct drm_mode_config mode_config;

    int (*dumb_create)(struct drm_mode_create_dumb* args);
};

int drm_mode_object_add(struct drm_driver* drv, struct drm_mode_object* obj,
                        unsigned int type);
struct drm_mode_object* drm_mode_object_find(struct drm_driver* drv,
                                             unsigned int id, unsigned type);

int drm_encoder_init(struct drm_driver* drv, struct drm_encoder* encoder,
                     int encoder_type);

#define obj_to_encoder(x) list_entry(x, struct drm_encoder, base)
static inline struct drm_encoder* drm_encoder_find(struct drm_driver* dev,
                                                   unsigned int id)
{
    struct drm_mode_object* mo;
    mo = drm_mode_object_find(dev, id, DRM_MODE_OBJECT_ENCODER);
    return mo ? obj_to_encoder(mo) : NULL;
}
#define drm_for_each_encoder(encoder, drv) \
    list_for_each_entry(encoder, &(drv)->mode_config.encoder_list, head)

int drm_connector_init(struct drm_driver* drv, struct drm_connector* connector,
                       int connector_type);
int drm_connector_attach_encoder(struct drm_connector* connector,
                                 struct drm_encoder* encoder);

#define obj_to_connector(x) list_entry(x, struct drm_connector, base)
static inline struct drm_connector* drm_connector_lookup(struct drm_driver* drv,
                                                         unsigned int id)
{
    struct drm_mode_object* mo;
    mo = drm_mode_object_find(drv, id, DRM_MODE_OBJECT_CONNECTOR);
    return mo ? obj_to_connector(mo) : NULL;
}

#define drm_for_each_connector(connector, drv) \
    list_for_each_entry(connector, &(drv)->mode_config.connector_list, head)

int drm_plane_init(struct drm_driver* drv, struct drm_plane* plane,
                   uint32_t possible_crtcs, enum drm_plane_type type);

int drm_framebuffer_init(struct drm_driver* drv, struct drm_framebuffer* fb);

#define obj_to_fb(x) list_entry(x, struct drm_framebuffer, base)
static inline struct drm_framebuffer*
drm_framebuffer_lookup(struct drm_driver* drv, unsigned int id)
{
    struct drm_mode_object* mo;
    mo = drm_mode_object_find(drv, id, DRM_MODE_OBJECT_FB);
    return mo ? obj_to_fb(mo) : NULL;
}

int drm_crtc_init_with_planes(struct drm_driver* drv, struct drm_crtc* crtc,
                              struct drm_plane* primary,
                              struct drm_plane* cursor,
                              const struct drm_crtc_funcs* funcs);

#define obj_to_crtc(x) list_entry(x, struct drm_crtc, base)
static inline struct drm_crtc* drm_crtc_lookup(struct drm_driver* drv,
                                               unsigned int id)
{
    struct drm_mode_object* mo;
    mo = drm_mode_object_find(drv, id, DRM_MODE_OBJECT_CRTC);
    return mo ? obj_to_crtc(mo) : NULL;
}

#define drm_for_each_crtc(crtc, drv) \
    list_for_each_entry(crtc, &(drv)->mode_config.crtc_list, head)

void drm_mode_config_init(struct drm_driver* drm_driver);

int drm_add_dmt_modes(struct drm_connector* connector, int hdisplay,
                      int vdisplay);
int drm_mode_convert_umode(struct drm_display_mode* out,
                           const struct drm_mode_modeinfo* in);
void drm_mode_convert_to_umode(struct drm_mode_modeinfo* out,
                               const struct drm_display_mode* in);

int drm_gem_object_init(struct drm_gem_object* obj, size_t size);
struct drm_gem_object* drm_gem_object_lookup(u32 handle);
int drm_gem_handle_create(struct drm_gem_object* obj, unsigned int* handlep);
int drm_gem_dumb_map_offset(u32 handle, u64* offset);

int drmdriver_task(struct drm_driver* drm_driver);

#endif
