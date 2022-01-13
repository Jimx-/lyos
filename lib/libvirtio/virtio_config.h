#ifndef _VIRTIO_CONFIG_H_
#define _VIRTIO_CONFIG_H_

#include <uapi/lyos/virtio_config.h>

#include <libvirtio/virtio.h>

struct virtio_config_ops {
    void (*get)(struct virtio_device* vdev, unsigned offset, void* buf,
                unsigned len);
    void (*set)(struct virtio_device* vdev, unsigned offset, const void* buf,
                unsigned len);
    u8 (*get_status)(struct virtio_device* vdev);
    void (*set_status)(struct virtio_device* vdev, u8 status);
    void (*reset)(struct virtio_device* dev);
    int (*enable_irq)(struct virtio_device* vdev);
    int (*had_irq)(struct virtio_device* vdev);
    int (*get_features)(struct virtio_device* vdev);
    int (*finalize_features)(struct virtio_device* vdev);
    int (*find_vqs)(struct virtio_device* vdev, unsigned nvqs,
                    struct virtqueue* vqs[]);
    void (*del_vqs)(struct virtio_device* vdev);
};

static inline int virtio_find_vqs(struct virtio_device* vdev, unsigned nvqs,
                                  struct virtqueue* vqs[])
{
    return vdev->config->find_vqs(vdev, nvqs, vqs);
}

static inline int virtio_enable_irq(struct virtio_device* vdev)
{
    return vdev->config->enable_irq(vdev);
}

static inline int virtio_had_irq(struct virtio_device* vdev)
{
    return vdev->config->had_irq(vdev);
}

static inline void virtio_device_ready(struct virtio_device* vdev)
{
    unsigned int status = vdev->config->get_status(vdev);

    vdev->config->set_status(vdev, status | VIRTIO_CONFIG_S_DRIVER_OK);
}

static inline u8 virtio_cread8(struct virtio_device* vdev, unsigned offset)
{
    u8 v;

    vdev->config->get(vdev, offset, &v, sizeof(v));
    return v;
}

static inline u16 virtio_cread16(struct virtio_device* vdev, unsigned offset)
{
    u16 v;

    vdev->config->get(vdev, offset, &v, sizeof(v));
    return v;
}

static inline u32 virtio_cread32(struct virtio_device* vdev, unsigned offset)
{
    u32 v;

    vdev->config->get(vdev, offset, &v, sizeof(v));
    return v;
}

static inline u64 virtio_cread64(struct virtio_device* vdev, unsigned offset)
{
    u64 v;

    vdev->config->get(vdev, offset, &v, sizeof(v));
    return v;
}

static inline void virtio_cwrite32(struct virtio_device* vdev, unsigned offset,
                                   u32 val)
{
    vdev->config->set(vdev, offset, &val, sizeof(val));
}

#define virtio_cread(vdev, structname, member, ptr)                      \
    do {                                                                 \
        switch (sizeof(*ptr)) {                                          \
        case 1:                                                          \
            *(ptr) = virtio_cread8(vdev, offsetof(structname, member));  \
            break;                                                       \
        case 2:                                                          \
            *(ptr) = virtio_cread16(vdev, offsetof(structname, member)); \
            break;                                                       \
        case 4:                                                          \
            *(ptr) = virtio_cread32(vdev, offsetof(structname, member)); \
            break;                                                       \
        case 8:                                                          \
            *(ptr) = virtio_cread64(vdev, offsetof(structname, member)); \
            break;                                                       \
        default:                                                         \
            break;                                                       \
        }                                                                \
    } while (0)

#endif
