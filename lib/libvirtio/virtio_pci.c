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
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <asm/pci.h>
#include <uapi/lyos/virtio_ring.h>

#include <libvirtio/libvirtio.h>

#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_config.h"

static int vp_find_vqs_intx(struct virtio_device* vdev, unsigned nvqs,
                            struct virtqueue* vqs[])
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int i, retval;

    retval = irq_setpolicy(vpdev->irq, 0, &vpdev->irq_hook);
    if (retval) return retval;

    retval = irq_enable(&vpdev->irq_hook);
    if (retval) {
        irq_rmpolicy(&vpdev->irq_hook);
        return retval;
    }

    vpdev->intx_enabled = 1;
    for (i = 0; i < nvqs; i++) {
        retval = vpdev->setup_vq(vpdev, i, &vqs[i]);

        if (retval) {
            goto out_del_vqs;
        }
    }

    return 0;

out_del_vqs:
    vp_del_vqs(vdev);
    return retval;
}

int vp_enable_irq(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    return irq_enable(&vpdev->irq_hook);
}

static inline void vp_del_vp(struct virtqueue* vq)
{
    struct virtio_pci_device* vpdev = to_vp_device(vq->vdev);

    vpdev->del_vq(vpdev, vq);
}

void vp_del_vqs(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    struct virtqueue *vq, *n;

    list_for_each_entry_safe(vq, n, &vdev->vqs, list) { vp_del_vp(vq); }

    if (vpdev->intx_enabled) {
        irq_disable(&vpdev->irq_hook);
        irq_rmpolicy(&vpdev->irq_hook);

        vpdev->intx_enabled = 0;
    }
}

int vp_find_vqs(struct virtio_device* vdev, unsigned nvqs,
                struct virtqueue* vqs[])
{
    return vp_find_vqs_intx(vdev, nvqs, vqs);
}

struct virtio_device* virtio_pci_setup(u16 subdid, int skip)
{
    struct virtio_device* vdev;

    vdev = virtio_pci_modern_setup(subdid, skip);

    if (!vdev) {
        /* fallback to legacy */
        vdev = virtio_pci_legacy_setup(subdid, skip);
    }

    return vdev;
}
