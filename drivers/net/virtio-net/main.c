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
#include <lyos/vm.h>
#include <sys/mman.h>

#include <libvirtio/libvirtio.h>
#include <libsysfs/libsysfs.h>
#include <libnetdriver/libnetdriver.h>

#include "virtio_net.h"

#define NR_PACKETS 64

#define PKT_BUF_SIZE (NR_PACKETS * ETH_PACKET_MAX)

enum { RX_Q, TX_Q, CTRL_Q };

struct packet {
    struct list_head list;
    int index;
    struct virtio_net_hdr* hdr_vir;
    phys_bytes hdr_phys;
    char* buf_vir;
    phys_bytes buf_phys;
    size_t len;
};

static const char* name = "virtio-net";

static struct virtio_device* vdev;
static struct virtqueue** vqs;
static unsigned int num_vqs;

static int has_ctrl_vq;
static int has_ctrl_rx;

static char* pkt_buf_vir;
static phys_bytes pkt_buf_phys;
static struct virtio_net_hdr* hdrs_vir;
static phys_bytes hdrs_phys;
static struct packet* packets;
static unsigned int in_rx;

static DEF_LIST(free_list);
static DEF_LIST(recv_list);

static struct virtio_net_config net_config;

static struct virtio_feature features[] = {
    {"partial csum", VIRTIO_NET_F_CSUM, 0, 0},
    {"given mac", VIRTIO_NET_F_MAC, 0, 1},
    {"status ", VIRTIO_NET_F_STATUS, 0, 0},
    {"control channel", VIRTIO_NET_F_CTRL_VQ, 0, 1},
    {"control channel rx", VIRTIO_NET_F_CTRL_RX, 0, 0}};

static int virtio_net_init(unsigned int instance, netdriver_addr_t* addr);
static int virtio_net_send(struct netdriver_data* data, size_t len);
static ssize_t virtio_net_recv(struct netdriver_data* data, size_t max);
static void virtio_net_intr(unsigned int mask);

static const struct netdriver virtio_net_driver = {
    .ndr_name = "virtio-net",
    .ndr_init = virtio_net_init,
    .ndr_send = virtio_net_send,
    .ndr_recv = virtio_net_recv,
    .ndr_intr = virtio_net_intr,
};

static void virtio_net_fill_rx_buffers(void)
{
    struct umap_phys phys[2];
    int added = FALSE;

    while ((in_rx < NR_PACKETS / 2) && !list_empty(&free_list)) {
        struct packet* pkt = list_first_entry(&free_list, struct packet, list);

        list_del(&pkt->list);

        phys[0].phys_addr = pkt->hdr_phys;
        phys[0].phys_addr |= 1;
        phys[0].size = sizeof(struct virtio_net_hdr);

        phys[1].phys_addr = pkt->buf_phys;
        phys[1].phys_addr |= 1;
        phys[1].size = ETH_PACKET_MAX;

        virtqueue_add_buffers(vqs[RX_Q], phys, 2, pkt);
        in_rx++;
        added = TRUE;
    }

    if (added) virtqueue_kick(vqs[RX_Q]);
}

static int virtio_net_send(struct netdriver_data* data, size_t len)
{
    struct umap_phys phys[2];
    struct packet* pkt;

    assert(len <= ETH_PACKET_MAX);

    if (list_empty(&free_list)) return SUSPEND;

    pkt = list_first_entry(&free_list, struct packet, list);
    list_del(&pkt->list);

    netdriver_copyin(data, pkt->buf_vir, len);

    phys[0].phys_addr = pkt->hdr_phys;
    phys[0].size = sizeof(struct virtio_net_hdr);

    phys[1].phys_addr = pkt->buf_phys;
    phys[1].size = len;

    virtqueue_add_buffers(vqs[TX_Q], phys, 2, pkt);

    virtqueue_kick(vqs[TX_Q]);

    return 0;
}

static ssize_t virtio_net_recv(struct netdriver_data* data, size_t max)
{
    struct packet* pkt;
    size_t len;

    if (list_empty(&recv_list)) return SUSPEND;

    pkt = list_first_entry(&recv_list, struct packet, list);
    list_del(&pkt->list);

    assert(pkt->len >= sizeof(struct virtio_net_hdr));
    len = pkt->len - sizeof(struct virtio_net_hdr);
    if (len > max) len = max;

    netdriver_copyout(data, pkt->buf_vir, len);

    memset(pkt->hdr_vir, 0, sizeof(*pkt->hdr_vir));
    list_add(&pkt->list, &free_list);

    virtio_net_fill_rx_buffers();

    return len;
}

static void virtio_net_intr(unsigned int mask)
{
    struct packet* pkt;
    size_t len;

    if (virtio_had_irq(vdev)) {
        while (!virtqueue_get_buffer(vqs[RX_Q], &len, (void**)&pkt)) {
            pkt->len = len;
            list_add_tail(&pkt->list, &recv_list);
            in_rx--;
        }

        while (!virtqueue_get_buffer(vqs[TX_Q], NULL, (void**)&pkt)) {
            memset(pkt->hdr_vir, 0, sizeof(*pkt->hdr_vir));
            list_add(&pkt->list, &free_list);
        }
    }

    if (!list_empty(&recv_list)) netdriver_recv();
    if (!list_empty(&free_list)) netdriver_send();

    virtio_enable_irq(vdev);

    virtio_net_fill_rx_buffers();
}

static int virtio_net_init_vqs(void)
{
    int retval;

    num_vqs = 2;
    if (virtio_host_supports(vdev, VIRTIO_NET_F_CTRL_VQ)) num_vqs++;

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

static int virtio_net_config(netdriver_addr_t* hwaddr)
{
    int i;

    printl("%s: Virtio net config:\n", name);

    if (virtio_host_supports(vdev, VIRTIO_NET_F_CTRL_VQ)) {
        has_ctrl_vq = TRUE;
    }

    if (virtio_host_supports(vdev, VIRTIO_NET_F_CTRL_RX)) {
        has_ctrl_rx = TRUE;
    }

    printl("  Features: %cctrl_vq %cctrl_rx\n", has_ctrl_vq ? '+' : '-',
           has_ctrl_rx ? '+' : '-');

    virtio_cread(vdev, struct virtio_net_config, mac, (u64*)net_config.mac);

    if (virtio_host_supports(vdev, VIRTIO_NET_F_MAC)) {
        printl("  Mac: ");
        for (i = 0; i < 6; i++)
            printl("%02x%s", net_config.mac[i], i == 5 ? "\n" : ":");
    } else {
        printl("  No Mac\n");
    }
    memcpy(hwaddr->addr, net_config.mac, sizeof(hwaddr->addr));

    if (virtio_host_supports(vdev, VIRTIO_NET_F_STATUS)) {
        printl("  Status: %x\n", net_config.status);
    }

    return 0;
}

static int virtio_net_alloc_buffers(void)
{
    int i;

    pkt_buf_vir =
        mmap(NULL, PKT_BUF_SIZE, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (pkt_buf_vir == MAP_FAILED) {
        return ENOMEM;
    }

    if (umap(SELF, UMT_VADDR, (vir_bytes)pkt_buf_vir, PKT_BUF_SIZE,
             &pkt_buf_phys) != 0) {
        panic("%s: umap failed", name);
    }

    hdrs_vir =
        mmap(NULL, sizeof(*hdrs_vir) * NR_PACKETS, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);

    if (hdrs_vir == MAP_FAILED) {
        munmap(pkt_buf_vir, PKT_BUF_SIZE);
        return ENOMEM;
    }

    if (umap(SELF, UMT_VADDR, (vir_bytes)hdrs_vir,
             sizeof(*hdrs_vir) * NR_PACKETS, &hdrs_phys) != 0) {
        panic("%s: umap failed", name);
    }

    packets = calloc(NR_PACKETS, sizeof(*packets));
    if (!packets) {
        munmap(pkt_buf_vir, PKT_BUF_SIZE);
        munmap(hdrs_vir, sizeof(*hdrs_vir) * NR_PACKETS);
        return ENOMEM;
    }

    memset(pkt_buf_vir, 0, PKT_BUF_SIZE);
    memset(hdrs_vir, 0, sizeof(*hdrs_vir) * NR_PACKETS);

    for (i = 0; i < NR_PACKETS; i++) {
        struct packet* pkt = &packets[i];

        pkt->index = i;
        pkt->hdr_vir = &hdrs_vir[i];
        pkt->hdr_phys = hdrs_phys + i * sizeof(hdrs_vir[i]);
        pkt->buf_vir = pkt_buf_vir + i * ETH_PACKET_MAX;
        pkt->buf_phys = pkt_buf_phys + i * ETH_PACKET_MAX;
        list_add(&pkt->list, &free_list);
    }

    return 0;
}

static int virtio_net_probe(int instance, netdriver_addr_t* hwaddr)
{
    int retval;

    vdev =
        virtio_probe_device(0x0001, name, features,
                            sizeof(features) / sizeof(features[0]), instance);

    if (!vdev) return ENXIO;

    retval = virtio_net_init_vqs();
    if (retval) {
        goto out_free_dev;
    }

    retval = virtio_net_alloc_buffers();
    if (retval) {
        goto out_free_vqs;
    }

    virtio_net_config(hwaddr);

    virtio_net_fill_rx_buffers();

    virtio_device_ready(vdev);

    return 0;

out_free_vqs:
    vdev->config->del_vqs(vdev);
out_free_dev:
    virtio_free_device(vdev);
    vdev = NULL;

    return retval;
}

static int virtio_net_init(unsigned int instance, netdriver_addr_t* hwaddr)
{
    int retval;

    printl("%s: Virtio net driver is running\n", name);

    retval = virtio_net_probe(instance, hwaddr);

    if (retval == ENXIO) {
        panic("%s: no virtio-net device found", name);
    }

    return 0;
}

int main()
{
    netdriver_task(&virtio_net_driver);

    return 0;
}
