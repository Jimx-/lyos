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
#include <lyos/portio.h>
#include <lyos/interrupt.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>

#include <libvirtio/libvirtio.h>
#include <libdrmdriver/libdrmdriver.h>

#include "virtio_gpu.h"

#define DRIVER_NAME "virtio-gpu"

#define XRES_MIN 32
#define YRES_MIN 32

#define XRES_DEF 1024
#define YRES_DEF 768

#define XRES_MAX 8192
#define YRES_MAX 8192

static const char* name = DRIVER_NAME;
static int instance;
static struct virtio_device* vdev;
static struct drm_device drm_dev;

static struct virtio_gpu_config gpu_config;
static int has_virgl;
static int has_edid;

static struct virtqueue* control_q;
static struct virtqueue* cursor_q;

static struct virtio_gpu_ctrl_hdr* hdrs_vir;
static phys_bytes hdrs_phys;

static struct idr resource_idr;

struct virtio_feature features[] = {{"virgl", VIRTIO_GPU_F_VIRGL, 0, 0},
                                    {"edid", VIRTIO_GPU_F_EDID, 0, 0}};

struct virtio_gpu_output {
    int index;
    struct drm_crtc crtc;
    struct drm_connector connector;
    struct drm_encoder encoder;
    struct virtio_gpu_display_one info;
    int enabled;
} outputs[VIRTIO_GPU_MAX_SCANOUTS];
#define drm_crtc_to_virtio_gpu_output(x) \
    list_entry(x, struct virtio_gpu_output, crtc)

struct virtio_gpu_framebuffer {
    struct drm_framebuffer base;
};
#define to_virtio_gpu_framebuffer(x) \
    list_entry(x, struct virtio_gpu_framebuffer, base)

struct virtio_gpu_object_params {
    uint32_t format;
    uint32_t width;
    uint32_t height;
    unsigned long size;
    int dumb;
};

struct virtio_gpu_object {
    struct drm_gem_object base;
    unsigned int hw_res_handle;
    void* vaddr;
    phys_bytes paddr;
    int dumb;
    int created;
};
#define gem_to_virtio_gpu_obj(gobj) \
    list_entry((gobj), struct virtio_gpu_object, base)

static void virtio_gpu_interrupt_wait(void);

static int virtio_gpu_mode_dumb_create(struct drm_device* dev,
                                       struct drm_mode_create_dumb* args);

static struct drm_driver virtio_gpu_driver = {
    .name = DRIVER_NAME,
    .dumb_create = virtio_gpu_mode_dumb_create,
};

static uint32_t virtio_gpu_translate_format(uint32_t drm_fourcc)
{
    uint32_t format;

    switch (drm_fourcc) {
    case DRM_FORMAT_XRGB8888:
        format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
        break;
    case DRM_FORMAT_ARGB8888:
        format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
        break;
    case DRM_FORMAT_BGRX8888:
        format = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM;
        break;
    case DRM_FORMAT_BGRA8888:
        format = VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM;
        break;
    default:
        format = 0;
        break;
    }

    return format;
}

static int virtio_gpu_resource_id_get(unsigned int* id_p)
{
    int handle = idr_alloc(&resource_idr, NULL, 1, 0);
    if (handle < 0) return -handle;

    *id_p = handle;
    return 0;
}

static int virtio_gpu_cmd_get_display_info(void)
{
    struct umap_phys phys[2];
    struct virtio_gpu_resp_display_info resp;
    int i, retval;

    memset(hdrs_vir, 0, sizeof(*hdrs_vir));
    hdrs_vir->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(struct virtio_gpu_ctrl_hdr);

    /* resp buffer */
    if ((retval = umap(SELF, &resp, &phys[1].phys_addr)) != 0) {
        return retval;
    }
    phys[1].phys_addr |= 1;
    phys[1].size = sizeof(resp);

    virtqueue_add_buffers(control_q, phys, 2, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    for (i = 0; i < gpu_config.num_scanouts; i++) {
        outputs[i].info = resp.pmodes[i];
    }

    return 0;
}

static int virtio_gpu_cmd_get_edids(void)
{
    struct umap_phys phys[2];
    struct virtio_gpu_cmd_get_edid* cmd =
        (struct virtio_gpu_cmd_get_edid*)hdrs_vir;
    struct virtio_gpu_resp_edid resp;
    int i, retval;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_GET_EDID;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(struct virtio_gpu_cmd_get_edid);

    /* resp buffer */
    if ((retval = umap(SELF, &resp, &phys[1].phys_addr)) != 0) {
        return retval;
    }
    phys[1].phys_addr |= 1;
    phys[1].size = sizeof(resp);

    for (i = 0; i < gpu_config.num_scanouts; i++) {
        cmd->scanout = i;

        virtqueue_add_buffers(control_q, phys, 2, NULL);

        virtqueue_kick(control_q);

        virtio_gpu_interrupt_wait();
    }

    return 0;
}

static int
virtio_gpu_cmd_create_resource(struct virtio_gpu_object* bo,
                               struct virtio_gpu_object_params* params)
{
    struct umap_phys phys[1];
    struct virtio_gpu_resource_create_2d* cmd =
        (struct virtio_gpu_resource_create_2d*)hdrs_vir;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd->format = params->format;
    cmd->width = params->width;
    cmd->height = params->height;
    cmd->resource_id = bo->hw_res_handle;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(struct virtio_gpu_resource_create_2d);

    virtqueue_add_buffers(control_q, phys, 1, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    bo->created = TRUE;
    return 0;
}

static int virtio_gpu_cmd_resource_attach_backing(
    unsigned int res_id, struct virtio_gpu_mem_entry* ents, size_t nents)
{
    struct umap_phys phys[2];
    struct virtio_gpu_resource_attach_backing* cmd =
        (struct virtio_gpu_resource_attach_backing*)hdrs_vir;
    int retval;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    cmd->nr_entries = nents;
    cmd->resource_id = res_id;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(struct virtio_gpu_resource_attach_backing);

    /* resp buffer */
    if ((retval = umap(SELF, ents, &phys[1].phys_addr)) != 0) {
        return retval;
    }
    phys[1].size = sizeof(*ents) * nents;

    virtqueue_add_buffers(control_q, phys, 2, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    return 0;
}

static int virtio_gpu_object_attach(struct virtio_gpu_object* bo,
                                    struct virtio_gpu_mem_entry* ents,
                                    size_t nents)
{
    return virtio_gpu_cmd_resource_attach_backing(bo->hw_res_handle, ents,
                                                  nents);
}

static int virtio_gpu_cmd_transfer_to_host_2d(struct virtio_gpu_object* bo,
                                              uint64_t offset, uint32_t width,
                                              uint32_t height, uint32_t x,
                                              uint32_t y)
{
    struct umap_phys phys[1];
    struct virtio_gpu_transfer_to_host_2d* cmd =
        (struct virtio_gpu_transfer_to_host_2d*)hdrs_vir;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd->resource_id = bo->hw_res_handle;
    cmd->offset = offset;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->r.x = x;
    cmd->r.y = y;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(*cmd);

    virtqueue_add_buffers(control_q, phys, 1, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    return 0;
}

static int virtio_gpu_cmd_set_scanout(uint32_t scanout_id, uint32_t resource_id,
                                      uint32_t width, uint32_t height,
                                      uint32_t x, uint32_t y)
{
    struct umap_phys phys[1];
    struct virtio_gpu_set_scanout* cmd =
        (struct virtio_gpu_set_scanout*)hdrs_vir;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd->resource_id = resource_id;
    cmd->scanout_id = scanout_id;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->r.x = x;
    cmd->r.y = y;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(*cmd);

    virtqueue_add_buffers(control_q, phys, 1, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    return 0;
}

static int virtio_gpu_cmd_resource_flush(uint32_t resource_id, uint32_t width,
                                         uint32_t height, uint32_t x,
                                         uint32_t y)
{
    struct umap_phys phys[1];
    struct virtio_gpu_resource_flush* cmd =
        (struct virtio_gpu_resource_flush*)hdrs_vir;

    memset(cmd, 0, sizeof(*cmd));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd->resource_id = resource_id;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->r.x = x;
    cmd->r.y = y;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(*cmd);

    virtqueue_add_buffers(control_q, phys, 1, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    return 0;
}

static int virtio_gpu_mmap(struct drm_gem_object* obj, endpoint_t endpoint,
                           char* addr, size_t length, char** retaddr)
{
    struct virtio_gpu_object* bo = gem_to_virtio_gpu_obj(obj);

    char* mapped = mm_map_phys(endpoint, (void*)bo->paddr, length);
    if (mapped == MAP_FAILED) {
        return ENOMEM;
    }

    *retaddr = mapped;
    return 0;
}

static const struct drm_gem_object_funcs virtio_gpu_gem_obj_funcs = {
    .mmap = virtio_gpu_mmap,
};

static int virtio_gpu_object_create(struct virtio_gpu_object_params* params,
                                    struct virtio_gpu_object** bo_p)
{
    struct virtio_gpu_object* bo;
    struct virtio_gpu_mem_entry ent;
    int retval;

    bo = malloc(sizeof(*bo));
    if (!bo) return ENOMEM;
    memset(bo, 0, sizeof(*bo));

    retval = drm_gem_object_init(&bo->base, params->size);
    if (retval) goto err_free_obj;

    bo->base.funcs = &virtio_gpu_gem_obj_funcs;

    bo->vaddr =
        mmap(NULL, params->size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (bo->vaddr == MAP_FAILED) {
        retval = ENOMEM;
        goto err_free_obj;
    }

    if ((retval = umap(SELF, bo->vaddr, &bo->paddr)) != 0) {
        goto err_free_mem;
    }

    retval = virtio_gpu_resource_id_get(&bo->hw_res_handle);
    if (retval) goto err_free_mem;

    bo->dumb = params->dumb;

    retval = virtio_gpu_cmd_create_resource(bo, params);
    if (retval) goto err_free_mem;

    ent.addr = bo->paddr;
    ent.length = params->size;

    retval = virtio_gpu_object_attach(bo, &ent, 1);
    if (retval) goto err_free_mem;

    *bo_p = bo;
    return 0;

err_free_mem:
    munmap(bo->vaddr, params->size);
err_free_obj:
    free(bo);
    return retval;
}

static int virtio_gpu_gem_create(struct drm_device* dev,
                                 struct virtio_gpu_object_params* params,
                                 struct drm_gem_object** obj_p,
                                 unsigned int* handle_p)
{
    struct virtio_gpu_object* obj;
    unsigned int handle;
    int retval;

    retval = virtio_gpu_object_create(params, &obj);
    if (retval) return retval;

    retval = drm_gem_handle_create(dev, &obj->base, &handle);
    if (retval) return retval;

    *obj_p = &obj->base;
    *handle_p = handle;

    return 0;
}

static int virtio_gpu_mode_dumb_create(struct drm_device* dev,
                                       struct drm_mode_create_dumb* args)
{
    struct virtio_gpu_object_params params = {0};
    struct drm_gem_object* gobj;
    int retval;
    uint32_t pitch;

    if (args->bpp != 32) return EINVAL;

    pitch = args->width * 4;
    args->size = pitch * args->height;
    args->size = roundup(args->size, ARCH_PG_SIZE);

    params.format = virtio_gpu_translate_format(DRM_FORMAT_XRGB8888);
    params.width = args->width;
    params.height = args->height;
    params.size = args->size;
    params.dumb = TRUE;
    retval = virtio_gpu_gem_create(dev, &params, &gobj, &args->handle);
    if (retval) return retval;

    args->pitch = pitch;
    return retval;
}

static void virtio_gpu_interrupt_wait(void)
{
    MESSAGE msg;
    void* data;

    do {
        send_recv(RECEIVE, INTERRUPT, &msg);
    } while (!virtio_had_irq(vdev));

    while (!virtqueue_get_buffer(control_q, NULL, &data))
        ;
}

static int virtio_gpu_probe(int instance)
{
    int retval = 0;

    vdev =
        virtio_probe_device(0x0010, name, features,
                            sizeof(features) / sizeof(features[0]), instance);

    if (!vdev) return ENXIO;

    return retval;
}

static int virtio_gpu_init_vqs(void)
{
    struct virtqueue* vqs[2];
    int retval;

    retval = virtio_find_vqs(vdev, 2, vqs);
    if (retval) return retval;

    control_q = vqs[0];
    cursor_q = vqs[0];

    return 0;
}

static int virtio_gpu_config(void)
{
    printl("%s: Virtio GPU config:\n", name);

    if (virtio_host_supports(vdev, VIRTIO_GPU_F_VIRGL)) {
        has_virgl = TRUE;
    }

    if (virtio_host_supports(vdev, VIRTIO_GPU_F_EDID)) {
        has_edid = TRUE;
    }

    printl("  Features: %cvirgl %cedid\n", has_virgl ? '+' : '-',
           has_edid ? '+' : '_');

    virtio_cread(vdev, struct virtio_gpu_config, num_scanouts,
                 &gpu_config.num_scanouts);
    printl("  Num. scanouts: %d\n", gpu_config.num_scanouts);

    return 0;
}

static int virtio_gpu_alloc_requests(void)
{
    hdrs_vir = mmap(
        NULL, roundup(sizeof(*hdrs_vir), ARCH_PG_SIZE), PROT_READ | PROT_WRITE,
        MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (hdrs_vir == MAP_FAILED) {
        return ENOMEM;
    }

    if (umap(SELF, hdrs_vir, &hdrs_phys) != 0) {
        panic("%s: umap failed", name);
    }

    return 0;
}

static void virtio_gpu_primary_plane_update(struct drm_plane* plane,
                                            struct drm_plane_state* old_state)
{
    struct virtio_gpu_output* output = NULL;
    struct virtio_gpu_object* bo = NULL;

    if (plane->state->crtc) {
        output = drm_crtc_to_virtio_gpu_output(plane->state->crtc);
    }
    if (old_state->crtc) {
        output = drm_crtc_to_virtio_gpu_output(old_state->crtc);
    }
    if (!output) return;

    if (!plane->state->fb) {
        virtio_gpu_cmd_set_scanout(output->index, 0, plane->state->src_w >> 16,
                                   plane->state->src_h >> 16, 0, 0);
        return;
    }

    bo = gem_to_virtio_gpu_obj(plane->state->fb->obj[0]);

    if (bo->dumb) {
        virtio_gpu_cmd_transfer_to_host_2d(bo, 0, plane->state->src_w >> 16,
                                           plane->state->src_h >> 16, 0, 0);
    }

    if (plane->state->fb != old_state->fb ||
        plane->state->src_w != old_state->src_w ||
        plane->state->src_h != old_state->src_h ||
        plane->state->src_x != old_state->src_x ||
        plane->state->src_y != old_state->src_y) {
        virtio_gpu_cmd_set_scanout(
            output->index, bo->hw_res_handle, plane->state->src_w >> 16,
            plane->state->src_h >> 16, plane->state->src_x >> 16,
            plane->state->src_y >> 16);
    }

    virtio_gpu_cmd_resource_flush(
        bo->hw_res_handle, plane->state->src_w >> 16, plane->state->src_h >> 16,
        plane->state->src_x >> 16, plane->state->src_y >> 16);
}

static struct drm_plane_helper_funcs virtio_gpu_primary_helper_funcs = {
    .update = virtio_gpu_primary_plane_update,
};

static int virtio_gpu_plane_init(enum drm_plane_type type, int index,
                                 struct drm_plane** pplane)
{
    struct drm_plane* plane;
    struct drm_plane_helper_funcs* funcs;
    int retval;

    if (type == DRM_PLANE_TYPE_PRIMARY) {
        funcs = &virtio_gpu_primary_helper_funcs;
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

static int virtio_output_init(int index)
{
    struct virtio_gpu_output* output = &outputs[index];
    struct drm_plane* primary;
    int retval;

    output->index = index;

    retval = virtio_gpu_plane_init(DRM_PLANE_TYPE_PRIMARY, index, &primary);
    if (retval) return retval;

    drm_crtc_init_with_planes(&drm_dev, &output->crtc, primary, NULL);

    drm_connector_init(&drm_dev, &output->connector,
                       DRM_MODE_CONNECTOR_VIRTUAL);

    drm_encoder_init(&drm_dev, &output->encoder, DRM_MODE_ENCODER_VIRTUAL);
    output->encoder.possible_crtcs = 1U << index;
    output->encoder.crtc = &output->crtc;

    drm_connector_attach_encoder(&output->connector, &output->encoder);
    output->connector.encoder = &output->encoder;

    return 0;
}

static int virtio_gpu_fb_create(struct drm_device* dev,
                                const struct drm_mode_fb_cmd2* cmd,
                                struct drm_framebuffer** fbp)
{
    struct drm_gem_object* obj = NULL;
    struct virtio_gpu_framebuffer* fb;
    int retval;

    if (cmd->pixel_format != DRM_FORMAT_XRGB8888 &&
        cmd->pixel_format != DRM_FORMAT_ARGB8888)
        return ENOENT;

    obj = drm_gem_object_lookup(dev, cmd->handles[0]);
    if (!obj) return EINVAL;

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

static void virtio_gpu_mode_commit(struct drm_atomic_state* state)
{
    struct drm_device* dev = state->dev;

    drm_helper_commit_planes(dev, state);
}

struct drm_mode_config_funcs virtio_gpu_mode_funcs = {
    .fb_create = virtio_gpu_fb_create, .commit = virtio_gpu_mode_commit};

static void virtio_gpu_modeset_init(void)
{
    struct drm_mode_config* config = &drm_dev.mode_config;
    int i;

    drm_mode_config_init(&drm_dev);

    config->max_width = XRES_MAX;
    config->min_width = XRES_MIN;
    config->max_height = YRES_MAX;
    config->min_height = YRES_MIN;

    config->funcs = &virtio_gpu_mode_funcs;

    for (i = 0; i < gpu_config.num_scanouts; i++) {
        virtio_output_init(i);
    }

    drm_mode_config_reset(&drm_dev);
}

static int virtio_gpu_init(void)
{
    int retval;

    instance = 0;
    idr_init(&resource_idr);

    printl("%s: Virtio GPU driver is running\n", name);

    retval = virtio_gpu_probe(instance);

    if (retval == ENXIO) {
        panic("%s: no virtio-gpu device found", name);
    }

    retval = virtio_gpu_init_vqs();
    if (retval) {
        goto out_free_dev;
    }

    retval = virtio_gpu_alloc_requests();
    if (retval) {
        goto out_free_vqs;
    }

    retval = virtio_gpu_config();
    if (retval) {
        goto out_free_vqs;
    }

    virtio_gpu_modeset_init();

    virtio_device_ready(vdev);

    if (has_edid) {
        virtio_gpu_cmd_get_edids();
    }

    virtio_gpu_cmd_get_display_info();

    drm_device_init(&drm_dev, &virtio_gpu_driver, vdev->dev_id);

    retval = drmdriver_register_device(&drm_dev);
    if (retval) {
        goto out_free_vqs;
    }

    return 0;

out_free_vqs:
    vdev->config->del_vqs(vdev);
out_free_dev:
    virtio_free_device(vdev);
    vdev = NULL;

    return retval;
}

int main()
{
    int retval;

    serv_register_init_fresh_callback(virtio_gpu_init);
    serv_init();

    retval = drmdriver_task(&drm_dev);
    if (retval) {
        panic("%s: failed to start DRM driver task");
    }

    return 0;
}
