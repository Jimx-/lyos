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
#include <sys/mman.h>
#include <lyos/virtio_ring.h>

#include <libvirtio/libvirtio.h>

#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_config.h"

static u8 vp_read8(u8* addr) { return *(volatile u8*)addr; }
static u8 vp_write8(u8* addr, u8 val) { *(volatile u8*)addr = val; }

#define VP_READ_IO(bits)                           \
    static u##bits vp_read##bits(__le##bits* addr) \
    {                                              \
        return *(volatile u##bits*)addr;           \
    }

VP_READ_IO(32)
VP_READ_IO(16)

#define VP_WRITE_IO(bits)                                     \
    static void vp_write##bits(__le##bits* addr, u##bits val) \
    {                                                         \
        *(volatile u##bits*)addr = val;                       \
    }

VP_WRITE_IO(32)
VP_WRITE_IO(16)

static void vp_write64(__le32* lo, __le32* hi, u64 val)
{
    vp_write32(lo, (u32)val);
    vp_write32(hi, val >> 32);
}

static void vp_get(struct virtio_device* vdev, unsigned offset, void* buf,
                   unsigned len)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    u8 b;
    u16 w;
    u32 l;

    switch (len) {
    case 1:
        b = vp_read8(vpdev->device + offset);
        *(u8*)buf = b;
        break;
    case 2:
        w = vp_read16(vpdev->device + offset);
        *(u16*)buf = w;
        break;
    case 4:
        l = vp_read32(vpdev->device + offset);
        *(u32*)buf = l;
        break;
    case 8:
        l = vp_read32(vpdev->device + offset);
        *(u32*)buf = l;
        l = vp_read32(vpdev->device + offset + sizeof(l));
        *((u32*)buf + 1) = l;
    }
}

static void vp_set(struct virtio_device* vdev, unsigned offset, const void* buf,
                   unsigned len)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    u8 b;
    u16 w;
    u32 l;

    switch (len) {
    case 1:
        b = *(u8*)buf;
        vp_write8(vpdev->device + offset, b);
        break;
    case 2:
        w = *(u16*)buf;
        vp_write16(vpdev->device + offset, w);
        break;
    case 4:
        l = *(u32*)buf;
        vp_write32(vpdev->device + offset, l);
        break;
    case 8:
        l = *(u32*)buf;
        vp_write32(vpdev->device + offset, l);
        l = *((u32*)buf + 1);
        vp_write32(vpdev->device + offset + sizeof(l), l);
    }
}

static u8 vp_get_status(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    return vp_read8(&vpdev->common->device_status);
}

static void vp_set_status(struct virtio_device* vdev, u8 status)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    vp_write8(&vpdev->common->device_status, status);
}

static void vp_reset(struct virtio_device* vdev)
{
    /* 0 = reset */
    vp_set_status(vdev, 0);
    /* flush out the status write */
    while (vp_get_status(vdev))
        ;
}

static int vp_get_features(struct virtio_device* vdev)
{
    u64 features;
    struct virtio_feature* f;
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int i;

    vp_write32(&vpdev->common->device_feature_select, 0);
    features = vp_read32(&vpdev->common->device_feature);
    vp_write32(&vpdev->common->device_feature_select, 1);
    features |= ((u64)vp_read32(&vpdev->common->device_feature) << 32);

    for (i = 0; i < vdev->num_features; i++) {
        f = &vdev->features[i];

        f->host_support = ((features >> f->bit) & 1);
    }

    return 0;
}

static int vp_finalize_features(struct virtio_device* vdev)
{
    u64 features;
    struct virtio_feature* f;
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int i;

    features = 0;
    for (i = 0; i < vdev->num_features; i++) {
        f = &vdev->features[i];

        features |= (f->guest_support << f->bit);
    }

    vp_write32(&vpdev->common->guest_feature_select, 0);
    vp_write32(&vpdev->common->guest_feature, (u32)features);
    vp_write32(&vpdev->common->guest_feature_select, 1);
    vp_write32(&vpdev->common->guest_feature, features >> 32);

    return 0;
}

static int vp_had_irq(struct virtio_device* vdev)
{
    struct virtio_pci_device* vpdev = to_vp_device(vdev);

    return vp_read8(vpdev->isr) & 1;
}

static int vp_modern_find_vqs(struct virtio_device* vdev, unsigned nvqs,
                              struct virtqueue* vqs[])
{
    struct virtqueue* vq;
    struct virtio_pci_device* vpdev = to_vp_device(vdev);
    int retval;

    retval = vp_find_vqs(vdev, nvqs, vqs);
    if (retval) return retval;

    list_for_each_entry(vq, &vdev->vqs, list)
    {
        vp_write16(&vpdev->common->queue_select, vq->index);
        vp_write16(&vpdev->common->queue_enable, 1);
    }

    return 0;
}

static const struct virtio_config_ops virtio_pci_config_nodev_ops = {
    .get = NULL,
    .set = NULL,
    .get_status = vp_get_status,
    .set_status = vp_set_status,
    .reset = vp_reset,
    .had_irq = vp_had_irq,
    .get_features = vp_get_features,
    .finalize_features = vp_finalize_features,
    .find_vqs = vp_modern_find_vqs,
    .del_vqs = vp_del_vqs,
};

static const struct virtio_config_ops virtio_pci_config_ops = {
    .get = vp_get,
    .set = vp_set,
    .get_status = vp_get_status,
    .set_status = vp_set_status,
    .reset = vp_reset,
    .had_irq = vp_had_irq,
    .get_features = vp_get_features,
    .finalize_features = vp_finalize_features,
    .find_vqs = vp_modern_find_vqs,
    .del_vqs = vp_del_vqs,
};

static int vp_notify(struct virtqueue* vq)
{
    vp_write16(vq->priv, vq->index);

    return TRUE;
}

static void* map_capability(int devind, int off, size_t minlen, u32 align,
                            u32 start, u32 size, size_t* len)
{
    u8 bar;
    u32 offset, length;
    u32 bar_base, bar_size;
    int ioflag, retval;
    void* p;

    bar = pci_attr_r8(devind, off + VIRTIO_PCI_CAP_BAR);
    offset = pci_attr_r32(devind, off + VIRTIO_PCI_CAP_OFFSET);
    length = pci_attr_r32(devind, off + VIRTIO_PCI_CAP_LENGTH);

    retval =
        pci_get_bar(devind, PCI_BAR + bar * 4, &bar_base, &bar_size, &ioflag);
    if (retval) return NULL;
    if (ioflag) return NULL;

    if (length <= start || length - start < minlen) {
        return NULL;
    }

    length -= start;

    if (start + offset < offset) {
        return NULL;
    }

    offset += start;

    if (offset & (align - 1)) {
        return NULL;
    }

    if (length > size) length = size;

    if (len) *len = length;

    if (minlen + offset < minlen || minlen + offset > bar_size) {
        return NULL;
    }

    p = mm_map_phys(SELF, (void*)(bar_base + offset), length);

    return p;
}

static int setup_vq(struct virtio_pci_device* vpdev, unsigned index,
                    struct virtqueue** vqp)
{
    struct virtqueue* vq;
    struct virtio_pci_common_cfg* cfg = vpdev->common;
    u16 num, off;
    phys_bytes desc_addr, avail_addr, used_addr;
    int retval;

    if (index >= vp_read16(&cfg->num_queues)) {
        return ENOENT;
    }

    vp_write16(&cfg->queue_select, index);

    num = vp_read16(&cfg->queue_size);
    if (!num || vp_read16(&cfg->queue_enable)) {
        return ENOENT;
    }

    if (num & (num - 1)) {
        return EINVAL;
    }

    off = vp_read16(&cfg->queue_notify_off);

    vq = vring_create_virtqueue(&vpdev->vdev, index, num, vp_notify);
    if (!vq) {
        return ENOMEM;
    }

    vp_write16(&cfg->queue_size, vq->num);

    desc_addr = vq->paddr;
    avail_addr = vq->paddr + ((char*)vq->vring.avail - (char*)vq->vring.desc);
    used_addr = vq->paddr + ((char*)vq->vring.used - (char*)vq->vring.desc);

    vp_write64(&cfg->queue_desc_lo, &cfg->queue_desc_hi, desc_addr);
    vp_write64(&cfg->queue_avail_lo, &cfg->queue_avail_hi, avail_addr);
    vp_write64(&cfg->queue_used_lo, &cfg->queue_used_hi, used_addr);

    if (vpdev->notify_base) {
        if ((u64)off * vpdev->notify_offset_multiplier + 2 >
            vpdev->notify_len) {
            retval = EINVAL;
            goto err_del_queue;
        }
        vq->priv = vpdev->notify_base + off * vpdev->notify_offset_multiplier;
    } else {
        vq->priv =
            map_capability(vpdev->devind, vpdev->notify_map_cap, 2, 2,
                           off * vpdev->notify_offset_multiplier, 2, NULL);
    }

    if (!vq->priv) {
        retval = ENOMEM;
        goto err_del_queue;
    }

    *vqp = vq;
    return 0;

err_del_queue:
    vring_del_virtqueue(vq);
    return retval;
}

static void del_vq(struct virtio_pci_device* vpdev, struct virtqueue* vq)
{
    vp_write16(&vpdev->common->queue_select, vq->index);

    if (!vpdev->notify_base) munmap(vq->priv, 2);

    vring_del_virtqueue(vq);
}

static inline int virtio_pci_find_capability(int devind, u8 cfg_type,
                                             int ioflag, int* bars)
{
    int pos;
    u8 type, bar;
    u32 base, size;
    int iof, retval;

    for (pos = pci_find_capability(devind, PCI_CAP_ID_VNDR); pos > 0;
         pos = pci_find_next_capability(devind, pos, PCI_CAP_ID_VNDR)) {

        type = pci_attr_r8(devind, pos + VIRTIO_PCI_CAP_CFG_TYPE);
        bar = pci_attr_r8(devind, pos + VIRTIO_PCI_CAP_BAR);

        if (bar > 0x5) continue;

        if (type == cfg_type) {
            retval = pci_get_bar(devind, PCI_BAR + bar * 4, &base, &size, &iof);

            if (retval == 0 && size > 0 && iof == ioflag) {
                *bars |= 1 << bar;
                return pos;
            }
        }
    }

    return 0;
}

struct virtio_device* virtio_pci_modern_setup(u16 subdid, int skip)
{
    int retval;
    int devind;
    u16 vid, did, sdid;
    device_id_t dev_id;
    struct virtio_pci_device* vpdev;
    int common, isr, notify, device;
    u32 notify_offset, notify_length;

    if (skip < 0) {
        return NULL;
    }

    retval = pci_first_dev(&devind, &vid, &did, &dev_id);

    while (!retval) {
        if (did < 0x1040) {
            /* transitional devices */
            sdid = pci_attr_r16(devind, PCI_SUBDID);
        } else {
            sdid = did - 0x1040;
        }

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
    vpdev->devind = devind;
    vpdev->vdev.dev_id = dev_id;

    common = virtio_pci_find_capability(devind, VIRTIO_PCI_CAP_COMMON_CFG, 0,
                                        &vpdev->modern_bars);
    if (!common) {
        goto err_free_dev;
    }

    isr = virtio_pci_find_capability(devind, VIRTIO_PCI_CAP_ISR_CFG, 0,
                                     &vpdev->modern_bars);
    notify = virtio_pci_find_capability(devind, VIRTIO_PCI_CAP_NOTIFY_CFG, 0,
                                        &vpdev->modern_bars);
    if (!isr || !notify) {
        goto err_free_dev;
    }

    device = virtio_pci_find_capability(devind, VIRTIO_PCI_CAP_DEVICE_CFG, 0,
                                        &vpdev->modern_bars);

    vpdev->common =
        map_capability(devind, common, sizeof(struct virtio_pci_common_cfg), 4,
                       0, sizeof(struct virtio_pci_common_cfg), NULL);
    if (!vpdev->common) goto err_free_dev;

    vpdev->isr = map_capability(devind, isr, sizeof(u8), 1, 0, 1, NULL);
    if (!vpdev->isr) goto err_unmap_common;

    vpdev->notify_offset_multiplier =
        pci_attr_r32(devind, notify + VIRTIO_PCI_NOTIFY_CAP_MULT);
    notify_offset = pci_attr_r32(devind, notify + VIRTIO_PCI_CAP_OFFSET);
    notify_length = pci_attr_r32(devind, notify + VIRTIO_PCI_CAP_LENGTH);

    if ((u64)notify_length + (notify_offset % ARCH_PG_SIZE) <= ARCH_PG_SIZE) {
        vpdev->notify_base = map_capability(devind, notify, 2, 2, 0,
                                            notify_length, &vpdev->notify_len);
        if (!vpdev->notify_base) goto err_unmap_isr;
    } else {
        vpdev->notify_map_cap = notify;
    }

    if (device) {
        vpdev->device = map_capability(devind, device, 0, 4, 0, ARCH_PG_SIZE,
                                       &vpdev->device_len);
        if (!vpdev->device) {
            goto err_unmap_notify;
        }

        vpdev->vdev.config = &virtio_pci_config_ops;
    } else {
        vpdev->vdev.config = &virtio_pci_config_nodev_ops;
    }

    vpdev->irq = pci_attr_r8(devind, PCI_ILR);
    vpdev->msix_enabled = 0;
    vpdev->intx_enabled = 0;

    vpdev->setup_vq = setup_vq;
    vpdev->del_vq = del_vq;

    printl("virtio: discovered PCI modern device, common 0x%x, irq %d\n",
           vpdev->common, vpdev->irq);

    return &vpdev->vdev;

err_unmap_notify:
    if (vpdev->notify_base) munmap(vpdev->notify_base, vpdev->notify_len);
err_unmap_isr:
    munmap(vpdev->isr, sizeof(u8));
err_unmap_common:
    munmap(vpdev->common, sizeof(struct virtio_pci_common_cfg));
err_free_dev:
    free(vpdev);
    return NULL;
}
