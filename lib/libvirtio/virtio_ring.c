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
#include <sys/mman.h>
#include <asm/proto.h>

#include <uapi/lyos/virtio_ring.h>

#include <libvirtio/libvirtio.h>

#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_config.h"

struct virtqueue* vring_create_virtqueue(struct virtio_device* vdev,
                                         unsigned int index, unsigned int num,
                                         int (*notify)(struct virtqueue*))
{
    struct virtqueue* vq;
    int i;

    if (num & (num - 1)) {
        printl("%s: bad queue length: %d\n", vdev->name, num);
        return NULL;
    }

    vq = malloc(sizeof(*vq));
    if (!vq) return NULL;

    memset(vq, 0, sizeof(*vq));
    vq->index = index;
    vq->num = num;
    vq->ring_size = vring_size(vq->num, ARCH_PG_SIZE);

    vq->vaddr =
        mmap(NULL, vq->ring_size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (vq->vaddr == MAP_FAILED) {
        goto out_free_vq;
    }

    if (umap(SELF, UMT_VADDR, (vir_bytes)vq->vaddr, vq->ring_size,
             &vq->paddr) != 0) {
        panic("%s: umap failed", vdev->name);
    }

    vq->data = calloc(vq->num, sizeof(vq->data[0]));
    if (!vq->data) {
        goto out_free_ring;
    }

    INIT_LIST_HEAD(&vq->list);
    memset(vq->vaddr, 0, vq->ring_size);
    memset(vq->data, 0, sizeof(vq->data[0]) * vq->num);
    vring_init(&vq->vring, vq->num, vq->vaddr, ARCH_PG_SIZE);

    list_add(&vq->list, &vdev->vqs);
    vq->vdev = vdev;
    vq->notify = notify;

    vq->vring.used->flags = 0;
    vq->vring.avail->flags = 0;
    vq->vring.used->idx = 0;
    vq->vring.avail->idx = 0;

    for (i = 0; i < vq->num; i++) {
        vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
        vq->vring.desc[i].next = (i + 1) & (vq->num - 1);

        vq->vring.avail->ring[i] = 0xffff;
        vq->vring.used->ring[i].id = 0xffff;
    }

    vq->free_num = vq->num;
    vq->free_head = 0;
    vq->free_tail = vq->num - 1;
    vq->last_used = 0;

    return vq;

out_free_ring:
    munmap(vq->vaddr, vq->ring_size);
out_free_vq:
    free(vq);
    return NULL;
}

void vring_del_virtqueue(struct virtqueue* vq)
{
    munmap(vq->vaddr, vq->ring_size);
    free(vq->data);
    free(vq);
}

static inline void use_vring_desc(struct vring_desc* vd, struct umap_phys* vp)
{
    vd->addr = vp->phys_addr & ~1UL;
    vd->len = vp->size;
    vd->flags = VRING_DESC_F_NEXT;

    if (vp->phys_addr & 1) vd->flags |= VRING_DESC_F_WRITE;
}

static void set_direct_descriptors(struct virtqueue* vq, struct umap_phys* bufs,
                                   size_t count)
{
    u16 i;
    size_t j;
    struct vring* vring = &vq->vring;
    struct vring_desc* vd;

    if (0 == count) return;

    for (i = vq->free_head, j = 0; j < count; j++) {
        vd = &vring->desc[i];

        use_vring_desc(vd, &bufs[j]);
        i = vd->next;
    }

    vd->flags = vd->flags & ~VRING_DESC_F_NEXT;

    vq->free_num -= count;
    vq->free_head = i;
}

int virtqueue_add_buffers(struct virtqueue* vq, struct umap_phys* bufs,
                          size_t count, void* data)
{
    struct vring* vring = &vq->vring;
    u16 old_head;

    old_head = vq->free_head;

    set_direct_descriptors(vq, bufs, count);

    vring->avail->ring[vring->avail->idx % vq->num] = old_head;
    vq->data[old_head] = data;

    cmb();
    vring->avail->idx++;
    cmb();

    return 0;
}

int virtqueue_get_buffer(struct virtqueue* vq, size_t* len, void** data)
{
    struct vring* vring = &vq->vring;
    struct vring_used_elem* used_elem;
    struct vring_desc* vd;
    u16 used_idx, vd_idx;
    int count = 0;

    cmb();

    used_idx = vring->used->idx;
    if (vq->last_used == used_idx) {
        return -1;
    }

    used_elem = &vring->used->ring[vq->last_used % vq->num];
    vq->last_used = vq->last_used + 1;

    vd_idx = used_elem->id % vq->num;
    vd = &vring->desc[vd_idx];

    vring->desc[vq->free_tail].next = vd_idx;

    while (vd->flags & VRING_DESC_F_NEXT) {
        vd_idx = vd->next;
        vd = &vring->desc[vd_idx];
        count++;
    }

    count++;

    vq->free_tail = vd_idx;
    vring->desc[vq->free_tail].next = vq->free_head;
    vring->desc[vq->free_tail].flags = VRING_DESC_F_NEXT;

    vq->free_num += count;

    *data = vq->data[used_elem->id];
    vq->data[used_elem->id] = NULL;

    if (len) {
        *len = used_elem->len;
    }

    return 0;
}

int virtqueue_notify(struct virtqueue* vq) { return vq->notify(vq); }

int virtqueue_kick(struct virtqueue* vq)
{
    cmb();

    if (!(vq->vring.used->flags & VRING_USED_F_NO_NOTIFY))
        return virtqueue_notify(vq);
    else
        return FALSE;
}
