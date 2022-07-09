#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <lyos/service.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <lyos/idr.h>
#include <asm/page.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>

#include <libdevman/libdevman.h>
#include <libdrmdriver/libdrmdriver.h>

#include "fb.h"

#define DRIVER_NAME "fbdrm"
#define DRIVER_DESC "FB DRM"
#define DRIVER_DATE "0"

#define DRIVER_MAJOR      0
#define DRIVER_MINOR      1
#define DRIVER_PATCHLEVEL 0

static const char* name = DRIVER_NAME;

static struct drm_device drm_dev;

static struct fbdrm_output {
    struct fb_info* fb_info;
    struct drm_crtc crtc;
    struct drm_connector connector;
    struct drm_encoder encoder;
} fbdrm_output;
#define drm_crtc_to_fbdrm_output(x) list_entry(x, struct fbdrm_output, crtc)
#define drm_connector_to_fbdrm_output(x) \
    list_entry(x, struct fbdrm_output, connector)

struct fbdrm_framebuffer {
    struct drm_framebuffer base;
};
#define to_fbdrm_framebuffer(x) list_entry(x, struct fbdrm_framebuffer, base)

struct fbdrm_object_params {
    uint32_t width;
    uint32_t height;
    unsigned long size;
    int dumb;
};

struct fbdrm_object {
    struct drm_gem_object base;
    void* vaddr;
    phys_bytes paddr;
    int dumb;
    int created;
};
#define gem_to_fbdrm_obj(gobj) list_entry((gobj), struct fbdrm_object, base)

static int fbdrm_mode_dumb_create(struct drm_device* dev,
                                  struct drm_mode_create_dumb* args);

static struct drm_driver fbdrm_driver = {
    .name = DRIVER_NAME,
    .desc = DRIVER_DESC,
    .date = DRIVER_DATE,

    .major = DRIVER_MAJOR,
    .minor = DRIVER_MINOR,
    .patchlevel = DRIVER_PATCHLEVEL,

    .dumb_create = fbdrm_mode_dumb_create,
};

static int probe_kernel_fb(struct fb_info** fbp)
{
    struct kinfo kinfo;
    void* fb_base;
    size_t screen_size;
    struct fb_info* fb;
    struct fb_videomode* mode;
    int ret = 0;

    get_kinfo(&kinfo);

    if (!kinfo.fb_base_phys) return ENXIO;

    fb = malloc(sizeof(*fb));
    if (!fb) return ENOMEM;

    ret = ENOMEM;
    mode = malloc(sizeof(*mode));
    if (!mode) goto out_free_fb;

    memset(fb, 0, sizeof(*fb));
    memset(mode, 0, sizeof(*mode));

    screen_size = kinfo.fb_screen_pitch * kinfo.fb_screen_height;
    fb_base = mm_map_phys(SELF, kinfo.fb_base_phys, screen_size, MMP_IO);
    if (fb_base == MAP_FAILED) goto out_free_mode;

    fb->screen_base = fb_base;
    fb->screen_base_phys = kinfo.fb_base_phys;
    fb->screen_size = screen_size;

    mode->xres = kinfo.fb_screen_width;
    mode->yres = kinfo.fb_screen_height;
    fb->mode = mode;

    *fbp = fb;

    return 0;

out_free_mode:
    free(mode);
out_free_fb:
    free(fb);
    return ret;
}

static int fbdrm_mmap(struct drm_gem_object* obj, endpoint_t endpoint,
                      char* addr, size_t length, char** retaddr)
{
    struct fbdrm_object* bo = gem_to_fbdrm_obj(obj);

    char* mapped = mm_map_phys(endpoint, bo->paddr, length, 0);
    if (mapped == MAP_FAILED) {
        return ENOMEM;
    }

    *retaddr = mapped;
    return 0;
}

static const struct drm_gem_object_funcs fbdrm_gem_obj_funcs = {
    .mmap = fbdrm_mmap,
};

static int fbdrm_object_create(struct fbdrm_object_params* params,
                               struct fbdrm_object** bo_p)
{
    struct fbdrm_object* bo;
    int retval;

    bo = malloc(sizeof(*bo));
    if (!bo) return ENOMEM;
    memset(bo, 0, sizeof(*bo));

    retval = drm_gem_object_init(&bo->base, params->size);
    if (retval) goto err_free_obj;

    bo->base.funcs = &fbdrm_gem_obj_funcs;

    bo->vaddr =
        mmap(NULL, params->size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (bo->vaddr == MAP_FAILED) {
        retval = ENOMEM;
        goto err_free_obj;
    }

    if ((retval = umap(SELF, UMT_VADDR, (vir_bytes)bo->vaddr, params->size,
                       &bo->paddr)) != 0) {
        goto err_free_mem;
    }

    bo->dumb = params->dumb;

    *bo_p = bo;
    return 0;

err_free_mem:
    munmap(bo->vaddr, params->size);
err_free_obj:
    free(bo);
    return retval;
}

static int fbdrm_gem_create(struct drm_device* dev,
                            struct fbdrm_object_params* params,
                            struct drm_gem_object** obj_p,
                            unsigned int* handle_p)
{
    struct fbdrm_object* obj;
    unsigned int handle;
    int retval;

    retval = fbdrm_object_create(params, &obj);
    if (retval) return retval;

    retval = drm_gem_handle_create(dev, &obj->base, &handle);
    if (retval) return retval;

    *obj_p = &obj->base;
    *handle_p = handle;

    return 0;
}

static int fbdrm_mode_dumb_create(struct drm_device* dev,
                                  struct drm_mode_create_dumb* args)
{
    struct fbdrm_object_params params = {0};
    struct drm_gem_object* gobj;
    int retval;
    uint32_t pitch;

    if (args->bpp != 32) {
        printl("BPP problem\n");
        return EINVAL;
    }

    pitch = args->width * 4;
    args->size = pitch * args->height;
    args->size = roundup(args->size, ARCH_PG_SIZE);

    params.width = args->width;
    params.height = args->height;
    params.size = args->size;
    params.dumb = TRUE;
    retval = fbdrm_gem_create(dev, &params, &gobj, &args->handle);
    if (retval) return retval;

    args->pitch = pitch;
    return retval;
}

static void fbdrm_primary_plane_update(struct drm_plane* plane,
                                       struct drm_plane_state* old_state)
{
    struct fbdrm_output* output = NULL;
    struct fbdrm_object* bo = NULL;

    if (plane->state->crtc) {
        output = drm_crtc_to_fbdrm_output(plane->state->crtc);
    }
    if (old_state->crtc) {
        output = drm_crtc_to_fbdrm_output(old_state->crtc);
    }
    if (!output) return;

    if (!plane->state->fb) {
        return;
    }

    bo = gem_to_fbdrm_obj(plane->state->fb->obj[0]);

    if (bo->dumb) {
        memcpy(output->fb_info->screen_base, bo->vaddr,
               output->fb_info->screen_size);
    }
}

static struct drm_plane_helper_funcs fbdrm_primary_helper_funcs = {
    .update = fbdrm_primary_plane_update,
};

static int fbdrm_plane_init(enum drm_plane_type type, int index,
                            struct drm_plane** pplane)
{
    struct drm_plane* plane;
    struct drm_plane_helper_funcs* funcs;
    int retval;

    if (type == DRM_PLANE_TYPE_PRIMARY) {
        funcs = &fbdrm_primary_helper_funcs;
    }

    plane = malloc(sizeof(*plane));
    if (!plane) {
        return ENOMEM;
    }

    memset(plane, 0, sizeof(*plane));

    retval = drm_plane_init(&drm_dev, plane, 1U << index, type);
    if (retval) goto err_free_plane;

    drm_plane_add_helper_funcs(plane, funcs);

    *pplane = plane;

    return 0;

err_free_plane:
    free(plane);
    return retval;
}

static int fbdrm_conn_get_modes(struct drm_connector* conn)
{
    struct fbdrm_output* output = drm_connector_to_fbdrm_output(conn);
    struct fb_info* fb = output->fb_info;
    int count;

    count = drm_add_dmt_modes(conn, fb->mode->xres, fb->mode->yres);

    drm_set_preferred_mode(conn, fb->mode->xres, fb->mode->yres);

    return count;
}

static const struct drm_connector_helper_funcs fbdrm_conn_helper_funcs = {
    .get_modes = fbdrm_conn_get_modes,
};

static int fbdrm_output_init(struct fb_info* fb)
{
    struct fbdrm_output* output = &fbdrm_output;
    struct drm_plane* primary;
    int retval;

    output->fb_info = fb;

    retval = fbdrm_plane_init(DRM_PLANE_TYPE_PRIMARY, 0, &primary);
    if (retval) return retval;

    drm_crtc_init_with_planes(&drm_dev, &output->crtc, primary, NULL);

    drm_connector_init(&drm_dev, &output->connector,
                       DRM_MODE_CONNECTOR_VIRTUAL);
    drm_connector_set_helper_funcs(&output->connector,
                                   &fbdrm_conn_helper_funcs);

    drm_encoder_init(&drm_dev, &output->encoder, DRM_MODE_ENCODER_VIRTUAL);
    output->encoder.possible_crtcs = 1U;
    output->encoder.crtc = &output->crtc;

    drm_connector_attach_encoder(&output->connector, &output->encoder);
    output->connector.encoder = &output->encoder;

    return 0;
}

static int fbdrm_fb_create(struct drm_device* dev,
                           const struct drm_mode_fb_cmd2* cmd,
                           struct drm_framebuffer** fbp)
{
    struct drm_gem_object* obj = NULL;
    struct fbdrm_framebuffer* fb;
    int retval;

    if (cmd->pixel_format != DRM_FORMAT_XRGB8888 &&
        cmd->pixel_format != DRM_FORMAT_ARGB8888)
        return ENOENT;

    obj = drm_gem_object_lookup(dev, cmd->handles[0]);
    if (!obj) {
        printl("Lookup problem\n");
        return EINVAL;
    }

    fb = malloc(sizeof(*fb));
    if (!fb) return ENOMEM;

    memset(fb, 0, sizeof(*fb));

    fb->base.obj[0] = obj;

    retval = drm_framebuffer_init(dev, &fb->base);
    if (retval) {
        fb->base.obj[0] = NULL;
        goto err_free_fb;
    }

    *fbp = &fb->base;

    return 0;

err_free_fb:
    free(fb);
    return retval;
}

static void fbdrm_mode_commit(struct drm_atomic_state* state)
{
    struct drm_device* dev = state->dev;

    drm_helper_commit_planes(dev, state);
}

struct drm_mode_config_funcs fbdrm_mode_funcs = {.fb_create = fbdrm_fb_create,
                                                 .commit = fbdrm_mode_commit};

static void fbdrm_modeset_init(struct fb_info* fb)
{
    struct drm_mode_config* config = &drm_dev.mode_config;

    drm_mode_config_init(&drm_dev);

    config->max_width = fb->mode->xres;
    config->min_width = fb->mode->xres;
    config->max_height = fb->mode->yres;
    config->min_height = fb->mode->yres;

    config->funcs = &fbdrm_mode_funcs;

    fbdrm_output_init(fb);

    drm_mode_config_reset(&drm_dev);
}

static int init_fbdrm()
{
    struct fb_info* fb;
    int retval;

    printl("%s: framebuffer DRM driver is running\n", name);

    retval = probe_kernel_fb(&fb);
    if (retval) return retval;

    fbdrm_modeset_init(fb);

    drm_device_init(&drm_dev, &fbdrm_driver, NO_DEVICE_ID);

    retval = drmdriver_register_device(&drm_dev);
    if (retval) return retval;

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(init_fbdrm);
    serv_init();

    return drmdriver_task(&drm_dev);
}
