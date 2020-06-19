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
#include <sys/mman.h>

#include <libblockdriver/libblockdriver.h>
#include <libvirtio/libvirtio.h>

#include "virtio_blk.h"

static const char* name = "virtio-blk";

static struct part_info primary[NR_PRIM_PER_DRIVE];
static struct part_info logical[NR_SUB_PER_DRIVE];

static struct virtio_device* vdev;
static struct virtqueue** vqs;
static unsigned int num_vqs;

struct virtio_blk_config blk_config;

static struct virtio_blk_outhdr* hdrs_vir;
static phys_bytes hdrs_phys;

#define status() ((*status_vir) & 0xff)
static u16* status_vir;
static phys_bytes status_phys;

struct virtio_feature features[] = {{"barrier", VIRTIO_BLK_F_BARRIER, 0, 0},
                                    {"sizemax", VIRTIO_BLK_F_SIZE_MAX, 0, 0},
                                    {"segmax", VIRTIO_BLK_F_SEG_MAX, 0, 0},
                                    {"geometry", VIRTIO_BLK_F_GEOMETRY, 0, 0},
                                    {"read-only", VIRTIO_BLK_F_RO, 0, 0},
                                    {"blocksize", VIRTIO_BLK_F_BLK_SIZE, 0, 0},
                                    {"scsi", VIRTIO_BLK_F_SCSI, 0, 0},
                                    {"flush", VIRTIO_BLK_F_FLUSH, 0, 0},
                                    {"topology", VIRTIO_BLK_F_TOPOLOGY, 0, 0},
                                    {"idbytes", VIRTIO_BLK_ID_BYTES, 0, 0}};

static ssize_t virtio_blk_rdwt(dev_t minor, int do_write, u64 pos,
                               endpoint_t endpoint, char* buf,
                               unsigned int count);
static struct part_info* virtio_blk_part(dev_t device);
static int virtio_blk_status2error(u8 status);
static void interrupt_wait();

static struct blockdriver virtio_blk_driver = {
    .bdr_readwrite = virtio_blk_rdwt,
    .bdr_part = virtio_blk_part,
};

static struct part_info* virtio_blk_part(dev_t device)
{
    int part_no = MINOR(device) % NR_SUB_PER_DRIVE;
    struct part_info* part = part_no < NR_PRIM_PER_DRIVE
                                 ? &primary[part_no]
                                 : &logical[part_no - NR_PRIM_PER_DRIVE];

    return part;
}

static ssize_t virtio_blk_rdwt(dev_t minor, int do_write, u64 pos,
                               endpoint_t endpoint, char* buf,
                               unsigned int count)
{
    struct part_info* part;
    u64 part_end, sector;
    struct umap_phys phys[3];
    int retval;

    part = virtio_blk_part(minor);

    if (!part) {
        return -ENXIO;
    }

    pos += part->base;

    if (pos % blk_config.blk_size) {
        return -EINVAL;
    }

    part_end = part->base + part->size;

    if (pos >= part_end) {
        return 0;
    }

    if (pos + count >= part_end) {
        count = part_end - pos;
    }

    if (count % blk_config.blk_size) {
        return -EINVAL;
    }

    sector = pos / blk_config.blk_size;

    if ((retval = umap(endpoint, buf, &phys[1].phys_addr)) != 0) {
        return -retval;
    }

    memset(hdrs_vir, 0, sizeof(*hdrs_vir));

    if (do_write) {
        hdrs_vir->type = VIRTIO_BLK_T_OUT;
    } else {
        hdrs_vir->type = VIRTIO_BLK_T_IN;
    }

    hdrs_vir->ioprio = 0;
    hdrs_vir->sector = sector;

    /* setup header */
    phys[0].phys_addr = hdrs_phys;
    phys[0].size = sizeof(struct virtio_blk_outhdr);

    /* physical buffer */
    phys[1].phys_addr |= !do_write;
    phys[1].size = count;

    /* status */
    phys[2].phys_addr = status_phys;
    phys[2].phys_addr |= 1;
    phys[2].size = sizeof(u8);

    virtqueue_add_buffers(vqs[0], phys, 3, NULL);

    virtqueue_kick(vqs[0]);

    interrupt_wait();

    if (status() == VIRTIO_BLK_S_OK) {
        return count;
    }

    return -virtio_blk_status2error(status());
}

static int virtio_blk_status2error(u8 status)
{
    switch (status) {
    case VIRTIO_BLK_S_IOERR:
        return EIO;
    case VIRTIO_BLK_S_UNSUPP:
        return ENOTSUP;
    default:
        panic("%s: unknown status %d", name, status);
    }

    return 0;
}

static void interrupt_wait()
{
    MESSAGE msg;
    void* data;

    do {
        send_recv(RECEIVE, INTERRUPT, &msg);
    } while (!virtio_had_irq(vdev));

    while (!virtqueue_get_buffer(vqs[0], NULL, &data))
        ;
}

static int virtio_blk_init_vqs(void)
{
    int retval;

    num_vqs = 1;

    vqs = calloc(num_vqs, sizeof(*vqs));

    if (!vqs) {
        return ENOMEM;
    }

    retval = virtio_find_vqs(vdev, num_vqs, vqs);

    if (retval) {
        free(vqs);
    }

    return retval;
}

static int virtio_blk_alloc_requests(void)
{
    hdrs_vir =
        mmap(NULL, sizeof(*hdrs_vir), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (hdrs_vir == MAP_FAILED) {
        return ENOMEM;
    }

    if (umap(SELF, hdrs_vir, &hdrs_phys) != 0) {
        panic("%s: umap failed", name);
    }

    status_vir =
        mmap(NULL, sizeof(*status_vir), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (status_vir == MAP_FAILED) {
        munmap(hdrs_vir, sizeof(*hdrs_vir));
        return ENOMEM;
    }

    if (umap(SELF, status_vir, &status_phys) != 0) {
        panic("%s: umap failed", name);
    }

    return 0;
}

static int virtio_blk_config(void)
{
    u32 size_mbs;

    printl("%s: Virtio block config:\n", name);

    virtio_cread(vdev, struct virtio_blk_config, capacity,
                 &blk_config.capacity);
    size_mbs = (blk_config.capacity << 9) >> 20;
    printl("  Capacity: %d MB\n", size_mbs);

    if (virtio_host_supports(vdev, VIRTIO_BLK_F_SEG_MAX)) {
        virtio_cread(vdev, struct virtio_blk_config, seg_max,
                     &blk_config.seg_max);

        printl("  Segment max: %d\n", blk_config.seg_max);
    }

    if (virtio_host_supports(vdev, VIRTIO_BLK_F_GEOMETRY)) {
        virtio_cread(vdev, struct virtio_blk_config, geometry.cylinders,
                     &blk_config.geometry.cylinders);
        virtio_cread(vdev, struct virtio_blk_config, geometry.heads,
                     &blk_config.geometry.heads);
        virtio_cread(vdev, struct virtio_blk_config, geometry.sectors,
                     &blk_config.geometry.sectors);

        printl("  Geometry: cylinders=%d heads=%d sectors=%d\n",
               blk_config.geometry.cylinders, blk_config.geometry.heads,
               blk_config.geometry.sectors);
    }

    if (virtio_host_supports(vdev, VIRTIO_BLK_F_BLK_SIZE)) {
        virtio_cread(vdev, struct virtio_blk_config, blk_size,
                     &blk_config.blk_size);

        printl("  Block size: %d\n", blk_config.blk_size);
    }
}

static char vbbuf[512];

static int virtio_blk_probe(int instance)
{
    int retval;

    vdev =
        virtio_probe_device(0x0002, name, features,
                            sizeof(features) / sizeof(features[0]), instance);

    if (!vdev) return ENXIO;

    retval = virtio_blk_init_vqs();
    if (retval) {
        goto out_free_dev;
    }

    retval = virtio_blk_alloc_requests();
    if (retval) {
        goto out_free_vqs;
    }

    virtio_blk_config();

    virtio_device_ready(vdev);

    primary[0].base = 0;
    primary[0].size = blk_config.capacity * blk_config.blk_size;

    return 0;

out_free_vqs:
    vdev->config->del_vqs(vdev);
out_free_dev:
    virtio_free_device(vdev);
    vdev = NULL;

    return retval;
}

static int virtio_blk_init(void)
{
    int retval;

    printl("%s: Virtio block driver is running\n", name);

    retval = virtio_blk_probe(0);

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(virtio_blk_init);
    serv_init();

    blockdriver_task(&virtio_blk_driver);

    return 0;
}
