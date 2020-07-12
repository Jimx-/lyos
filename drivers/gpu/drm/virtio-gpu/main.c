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

#include "virtio_gpu.h"

static const char* name = "virtio-gpu";
static int instance;
static struct virtio_device* vdev;

static struct virtqueue* control_q;
static struct virtqueue* cursor_q;

struct virtio_feature features[] = {{"virgl", VIRTIO_GPU_F_VIRGL, 0, 0},
                                    {"edid", VIRTIO_GPU_F_EDID, 0, 0}};

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
    MESSAGE msg;

    serv_register_init_fresh_callback(virtio_gpu_init);
    serv_init();

    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);
    }

    return 0;
}
