#ifndef _LIBVIRTIO_H_
#define _LIBVIRTIO_H_

#include <lyos/type.h>
#include <uapi/lyos/virtio_pci.h>

#include <libvirtio/virtio_config.h>

#define VIRTIO_VENDOR_ID 0x1AF4

struct virtio_feature {
    const char* name;
    int bit;
    int host_support;
    int guest_support;
};

struct virtio_device;
struct virtqueue;

struct virtio_device* virtio_probe_device(u16 subdid, const char* name,
                                          struct virtio_feature* features,
                                          int num_features, int skip);
void virtio_free_device(struct virtio_device* vdev);

int virtio_host_supports(struct virtio_device* dev, int bit);
int virtio_guest_supports(struct virtio_device* dev, int bit);

int virtqueue_add_buffers(struct virtqueue* vq, struct umap_phys* bufs,
                          size_t count, void* data);
int virtqueue_get_buffer(struct virtqueue* vq, size_t* len, void** data);
int virtqueue_kick(struct virtqueue* vq);

#endif
