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
#include <lyos/virtio_ring.h>

#include <libvirtio/libvirtio.h>

#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_config.h"

#define VP_READ_IO(bits, suffix)                                           \
    static u##bits vp_read##bits(struct virtio_pci_device* vpdev, int off) \
    {                                                                      \
        u32 val;                                                           \
        int retval;                                                        \
        if ((retval = portio_in##suffix(vpdev->port + off, &val)) != 0) {  \
            panic("%s: vp_read" #bits " failed %d (%d)", vpdev->vdev.name, \
                  vpdev->port, retval);                                    \
        }                                                                  \
        return val;                                                        \
    }

VP_READ_IO(32, l)
VP_READ_IO(16, w)
VP_READ_IO(8, b)

#define VP_WRITE_IO(bits, suffix)                                           \
    static void vp_write##bits(struct virtio_pci_device* vpdev, int off,    \
                               u##bits val)                                 \
    {                                                                       \
        int retval;                                                         \
        if ((retval = portio_out##suffix(vpdev->port + off, val)) != 0) {   \
            panic("%s: vp_write" #bits " failed %d (%d)", vpdev->vdev.name, \
                  vpdev->port, retval);                                     \
        }                                                                   \
    }

VP_WRITE_IO(32, l)
VP_WRITE_IO(16, w)
VP_WRITE_IO(8, b)

static void vp_get(struct virtio_device* vdev, unsigned offset, void* buf,
                   unsigned len)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    unsigned addr = VIRTIO_PCI_CONFIG_OFF(vpdev->msix_enabled) + offset;
    u8* ptr = buf;
    int i;

    for (i = 0; i < len; i++)
        ptr[i] = vp_read8(vpdev, addr + i);
}

static void vp_set(struct virtio_device* vdev, unsigned offset, const void* buf,
                   unsigned len)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    unsigned addr = VIRTIO_PCI_CONFIG_OFF(vpdev->msix_enabled) + offset;
    const u8* ptr = buf;
    int i;

    for (i = 0; i < len; i++)
        vp_write8(vpdev, addr + i, ptr[i]);
}

static u8 vp_get_status(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    return vp_read8(vpdev, VIRTIO_PCI_STATUS);
}

static void vp_set_status(struct virtio_device* vdev, u8 status)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    vp_write8(vpdev, VIRTIO_PCI_STATUS, status);
}

static void vp_reset(struct virtio_device* vdev)
{
    /* 0 = reset */
    vp_set_status(vdev, 0);
    /* flush out the status write */
    (void)vp_get_status(vdev);
}

static int vp_get_features(struct virtio_device* vdev)
{
    u32 features;
    struct virtio_feature* f;
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int i;

    features = vp_read32(vpdev, VIRTIO_PCI_HOST_FEATURES);

    for (i = 0; i < vdev->num_features; i++) {
        f = &vdev->features[i];

        f->host_support = ((features >> f->bit) & 1);
    }

    return 0;
}

static int vp_finalize_features(struct virtio_device* vdev)
{
    u32 features;
    struct virtio_feature* f;
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int i;

    features = 0;
    for (i = 0; i < vdev->num_features; i++) {
        f = &vdev->features[i];

        features |= (f->guest_support << f->bit);
    }

    vp_write32(vpdev, VIRTIO_PCI_GUEST_FEATURES, features);

    return 0;
}

static int vp_had_irq(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    return vp_read8(vpdev, VIRTIO_PCI_ISR) & 1;
}

static const struct virtio_config_ops virtio_pci_config_ops = {
    .get = vp_get,
    .set = vp_set,
    .get_status = vp_get_status,
    .set_status = vp_set_status,
    .reset = vp_reset,
    .enable_irq = vp_enable_irq,
    .had_irq = vp_had_irq,
    .get_features = vp_get_features,
    .finalize_features = vp_finalize_features,
    .find_vqs = vp_find_vqs,
    .del_vqs = vp_del_vqs,
};

static int vp_notify(struct virtqueue* vq)
{
    struct virtio_pci_device* vpdev = to_vp_device(vq->vdev);

    vp_write16(vpdev, VIRTIO_PCI_QUEUE_NOTIFY, vq->index);
    return TRUE;
}

static int setup_vq(struct virtio_pci_device* vpdev, unsigned index,
                    struct virtqueue** vqp)
{
    struct virtqueue* vq;
    u16 num;
    unsigned long q_pfn;

    vp_write16(vpdev, VIRTIO_PCI_QUEUE_SEL, index);
    num = vp_read16(vpdev, VIRTIO_PCI_QUEUE_NUM);

    if (!num || vp_read32(vpdev, VIRTIO_PCI_QUEUE_PFN)) {
        return ENOENT;
    }

    vq = vring_create_virtqueue(&vpdev->vdev, index, num, vp_notify);

    q_pfn = vq->paddr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
    vp_write32(vpdev, VIRTIO_PCI_QUEUE_PFN, q_pfn);

    *vqp = vq;

    return 0;
}

static void del_vq(struct virtio_pci_device* vpdev, struct virtqueue* vq)
{
    vp_write16(vpdev, VIRTIO_PCI_QUEUE_SEL, vq->index);
    vp_write32(vpdev, VIRTIO_PCI_QUEUE_PFN, 0);

    vring_del_virtqueue(vq);
}

struct virtio_device* virtio_pci_legacy_setup(u16 subdid, int skip)
{
    int retval;
    int devind;
    u16 vid, did, sdid;
    device_id_t dev_id;
    unsigned long base;
    size_t size;
    int ioflag;
    struct virtio_pci_device* vpdev;

    if (skip < 0) {
        return NULL;
    }

    retval = pci_first_dev(&devind, &vid, &did, &dev_id);

    while (!retval) {
        sdid = pci_attr_r16(devind, PCI_SUBDID);

        if (vid == VIRTIO_VENDOR_ID && subdid == sdid) {
            if (skip == 0) break;
            skip--;
        }

        retval = pci_next_dev(&devind, &vid, &did, &dev_id);
    }

    if (retval || skip) return NULL;

    vpdev = malloc(sizeof(*vpdev));

    if (!vpdev) return NULL;

    memset(vpdev, 0, sizeof(*vpdev));
    vpdev->vdev.config = &virtio_pci_config_ops;
    vpdev->vdev.dev_id = dev_id;

    retval = pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag);
    if (retval) goto err;
    if (!ioflag) goto err;

    vpdev->port = (u16)base;

    vpdev->irq = pci_attr_r8(devind, PCI_ILR);
    vpdev->msix_enabled = 0;
    vpdev->intx_enabled = 0;

    vpdev->setup_vq = setup_vq;
    vpdev->del_vq = del_vq;

    printl("virtio: discovered PCI legacy device, base 0x%x, irq %d\n",
           vpdev->port, vpdev->irq);

    return &vpdev->vdev;

err:
    free(vpdev);
    return NULL;
}
