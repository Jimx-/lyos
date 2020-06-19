#include <lyos/type.h>
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
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <uapi/lyos/virtio_ring.h>

#include <libvirtio/libvirtio.h>

#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_config.h"

void virtio_add_status(struct virtio_device* vdev, unsigned int status)
{
    vdev->config->set_status(vdev, vdev->config->get_status(vdev) | status);
}

static int virtio_exchange_features(struct virtio_device* vdev)
{
    int retval;
    retval = vdev->config->get_features(vdev);
    if (retval) return retval;

    return vdev->config->finalize_features(vdev);
}

static int _supports(struct virtio_device* vdev, int bit, int host)
{
    for (int i = 0; i < vdev->num_features; i++) {
        struct virtio_feature* f = &vdev->features[i];

        if (f->bit == bit) return host ? f->host_support : f->guest_support;
    }

    return 0;
}

int virtio_host_supports(struct virtio_device* vdev, int bit)
{
    return _supports(vdev, bit, TRUE);
}

int virtio_guest_supports(struct virtio_device* vdev, int bit)
{
    return _supports(vdev, bit, FALSE);
}

struct virtio_device* virtio_probe_device(u16 subdid, const char* name,
                                          struct virtio_feature* features,
                                          int num_features, int skip)
{
    int retval;
    struct virtio_device* vdev =
        virtio_pci_legacy_setup(subdid, name, features, num_features, skip);

    if (!vdev) return NULL;

    retval = virtio_exchange_features(vdev);
    if (retval) goto out_free;

    INIT_LIST_HEAD(&vdev->vqs);

    virtio_add_status(vdev, VIRTIO_CONFIG_S_DRIVER);

    return vdev;

out_free:
    free(vdev);
    return NULL;
}

void virtio_free_device(struct virtio_device* vdev) { free(vdev); }
