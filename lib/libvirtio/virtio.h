#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <lyos/list.h>
#include <uapi/lyos/virtio_ring.h>
#include <libdevman/libdevman.h>

struct virtio_device;

struct virtqueue {
    struct list_head list;

    void* vaddr;
    phys_bytes paddr;
    struct virtio_device* vdev;
    void* priv;

    unsigned int index;
    u16 num;
    unsigned int ring_size;
    struct vring vring;

    u16 free_num;
    u16 free_head;
    u16 free_tail;
    u16 last_used;

    int (*notify)(struct virtqueue*);

    void** data;
};

struct virtio_device {
    const char* name;

    struct virtio_feature* features;
    int num_features;

    device_id_t dev_id;

    struct list_head vqs;

    const struct virtio_config_ops* config;
};

void virtio_add_status(struct virtio_device* vdev, unsigned int status);

struct virtqueue* vring_create_virtqueue(struct virtio_device* vdev,
                                         unsigned int index, unsigned int num,
                                         int (*notify)(struct virtqueue*));
void vring_del_virtqueue(struct virtqueue* vq);

struct virtio_device* virtio_pci_setup(u16 subdid, int skip);

#endif
