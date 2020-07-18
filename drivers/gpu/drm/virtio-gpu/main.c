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

#include <libvirtio/libvirtio.h>
#include <libdrmdriver/libdrmdriver.h>

#include "virtio_gpu.h"

static const char* name = "virtio-gpu";
static int instance;
static struct virtio_device* vdev;

static struct virtio_gpu_config gpu_config;

static struct virtqueue* control_q;
static struct virtqueue* cursor_q;

static struct virtio_gpu_ctrl_hdr* hdrs_vir;
static phys_bytes hdrs_phys;

struct virtio_feature features[] = {{"virgl", VIRTIO_GPU_F_VIRGL, 0, 0},
                                    {"edid", VIRTIO_GPU_F_EDID, 0, 0}};

static void virtio_gpu_interrupt_wait(void);

static struct drm_driver virtio_gpu_driver;

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

    virtio_cread(vdev, struct virtio_gpu_config, num_scanouts,
                 &gpu_config.num_scanouts);
    printl("  Num. scanouts: %d\n", gpu_config.num_scanouts);

    return 0;
}

static int virtio_blk_alloc_requests(void)
{
    hdrs_vir =
        mmap(NULL, sizeof(*hdrs_vir), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (hdrs_vir == MAP_FAILED) {
        return ENOMEM;
    }

    if (umap(SELF, hdrs_vir, &hdrs_phys) != 0) {
        panic("%s: umap failed", name);
    }

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

    /* id string */
    if ((retval = umap(SELF, &resp, &phys[1].phys_addr)) != 0) {
        return retval;
    }
    phys[1].phys_addr |= 1;
    phys[1].size = sizeof(resp);

    virtqueue_add_buffers(control_q, phys, 2, NULL);

    virtqueue_kick(control_q);

    virtio_gpu_interrupt_wait();

    for (i = 0; i < gpu_config.num_scanouts; i++) {
        printl("%d %d\n", resp.pmodes[i].r.width, resp.pmodes[i].r.height);
    }

    return 0;
}

static int virtio_gpu_init(void)
{
    int retval;

    instance = 0;

    printl("%s: Virtio GPU driver is running\n", name);

    retval = virtio_gpu_probe(instance);

    if (retval == ENXIO) {
        panic("%s: no virtio-gpu device found", name);
    }

    retval = virtio_gpu_init_vqs();
    if (retval) {
        goto out_free_dev;
    }

    retval = virtio_blk_alloc_requests();
    if (retval) {
        goto out_free_vqs;
    }

    retval = virtio_gpu_config();
    if (retval) {
        goto out_free_vqs;
    }

    virtio_device_ready(vdev);

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

    retval = drmdriver_task(vdev->dev_id, &virtio_gpu_driver);
    if (retval) {
        panic("%s: failed to start DRM driver task");
    }

    return 0;
}
