#ifndef _VIRTIO_PCI_H_
#define _VIRTIO_PCI_H_

#include "virtio.h"

struct virtio_pci_device {
    struct virtio_device vdev;
    int devind;

    // modern-only
    u8* isr;
    struct virtio_pci_common_cfg* common;
    void* device;
    void* notify_base;

    size_t notify_len;
    size_t device_len;

    int notify_map_cap;

    u32 notify_offset_multiplier;

    int modern_bars;

    // legacy-only
    u16 port;

    int msix_enabled;
    int intx_enabled;
    int irq;
    int irq_hook;

    int (*setup_vq)(struct virtio_pci_device* vpdev, unsigned index,
                    struct virtqueue** vqp);
    void (*del_vq)(struct virtio_pci_device* vpdev, struct virtqueue* vq);
};

static inline struct virtio_pci_device* to_vp_device(struct virtio_device* vdev)
{
    return list_entry(vdev, struct virtio_pci_device, vdev);
}

int vp_find_vqs(struct virtio_device* vdev, unsigned nvqs,
                struct virtqueue* vqs[]);
void vp_del_vqs(struct virtio_device* vdev);

int vp_enable_irq(struct virtio_device* vdev);

struct virtio_device* virtio_pci_legacy_setup(u16 subdid, int skip);
struct virtio_device* virtio_pci_modern_setup(u16 subdid, int skip);

#endif
