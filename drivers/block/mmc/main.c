#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lyos/list.h>
#include <lyos/idr.h>

#include <libblockdriver/libblockdriver.h>

#include "mmc.h"
#include "mmchost.h"
#include "sdhci.h"

#define NAME "mmcblk"

#define MAX_THREADS 1
#define MAX_DEVICES 4

#if CONFIG_OF
void* boot_params;
#endif

struct mmc_blk_data {
    int devidx;
    struct mmc_card* card;
    int open_count;

    struct part_info primary[NR_PRIM_PER_DRIVE];
    struct part_info logical[NR_SUB_PER_DRIVE];
};

static class_id_t mmc_class;

static DEF_LIST(mmc_hosts);

static struct idr mmc_host_idr;
static struct idr mmc_blk_idr;

static int mmc_blk_open(dev_t minor, int access);
static int mmc_blk_close(dev_t minor);
static ssize_t mmc_blk_rdwt(dev_t minor, int do_write, loff_t pos,
                            endpoint_t endpoint, const struct iovec_grant* iov,
                            size_t count);
static struct part_info* mmc_blk_part(dev_t device);
static void mmc_blk_intr(unsigned mask);

static struct blockdriver mmc_blk_driver = {
    .bdr_open = mmc_blk_open,
    .bdr_close = mmc_blk_close,
    .bdr_readwrite = mmc_blk_rdwt,
    .bdr_part = mmc_blk_part,
    .bdr_intr = mmc_blk_intr,
};

static inline struct mmc_blk_data* mmc_blk_get(dev_t minor)
{
    int devidx = minor / NR_SUB_PER_DRIVE;
    return idr_find(&mmc_blk_idr, devidx);
}

static int mmc_blk_open(dev_t minor, int access)
{
    struct mmc_blk_data* md = mmc_blk_get(minor);

    if (!md) return EIO;

    md->open_count++;

    return 0;
}

static int mmc_blk_close(dev_t minor)
{
    struct mmc_blk_data* md = mmc_blk_get(minor);

    if (!md) return EIO;

    md->open_count++;

    return 0;
}

static struct part_info* mmc_blk_part(dev_t minor)
{
    struct mmc_blk_data* md = mmc_blk_get(minor);
    int part_no = minor % NR_SUB_PER_DRIVE;
    struct part_info* part;

    if (!md) return NULL;

    part = part_no < NR_PRIM_PER_DRIVE
               ? &md->primary[part_no]
               : &md->logical[part_no - NR_PRIM_PER_DRIVE];

    return part;
}

struct mmc_host* mmc_alloc_host(int extra)
{
    struct mmc_host* host;
    int index;

    host = malloc(sizeof(struct mmc_host) + extra);

    if (!host) return NULL;
    memset(host, 0, sizeof(struct mmc_host) + extra);

    index = idr_alloc(&mmc_host_idr, host, 0, 0);
    if (index < 0) {
        free(host);
        return NULL;
    }

    host->index = index;

    return host;
}

static inline void mmc_set_ios(struct mmc_host* host)
{
    host->ops->set_ios(host, &host->ios);
}

static void mmc_hw_reset_for_init(struct mmc_host* host)
{
    if (host->ops->hw_reset) host->ops->hw_reset(host);
}

void mmc_set_clock(struct mmc_host* host, unsigned int hz)
{
    host->ios.clock = hz;
    mmc_set_ios(host);
}

void mmc_set_bus_width(struct mmc_host* host, unsigned int width)
{
    host->ios.bus_width = width;
    mmc_set_ios(host);
}

void mmc_request_done(struct mmc_host* host, struct mmc_request* mrq)
{
    if (mrq->done) mrq->done(mrq);
}

static void mmc_wait_done(struct mmc_request* mrq)
{
    mrq->completed = TRUE;
    blockdriver_async_wakeup(mrq->tid);
}

static int mmc_start_request(struct mmc_host* host, struct mmc_request* mrq)
{
    if (mrq->cmd) {
        mrq->cmd->error = 0;
        mrq->cmd->mrq = mrq;
        mrq->cmd->data = mrq->data;
    }

    if (mrq->data) {
        if (mrq->data->len != mrq->data->blocks * mrq->data->blksz)
            return -EINVAL;

        mrq->data->error = 0;
        mrq->data->mrq = mrq;
    }

    host->ops->request(host, mrq);

    return 0;
}

void mmc_wait_for_req(struct mmc_host* host, struct mmc_request* mrq)
{
    int err;

    mrq->tid = blockdriver_async_worker_id();
    mrq->completed = FALSE;
    mrq->done = mmc_wait_done;

    err = mmc_start_request(host, mrq);
    if (err) {
        mrq->cmd->error = err;
        return;
    }

    if (!mrq->completed) blockdriver_async_sleep();
}

int mmc_wait_for_cmd(struct mmc_host* host, struct mmc_command* cmd)
{
    struct mmc_request mrq = {};

    mrq.cmd = cmd;
    cmd->data = NULL;

    mmc_wait_for_req(host, &mrq);

    return cmd->error;
}

int mmc_go_idle(struct mmc_host* host)
{
    struct mmc_command cmd = {};

    cmd.opcode = MMC_GO_IDLE_STATE;
    cmd.arg = 0;
    cmd.flags = MMC_RSP_NONE | MMC_CMD_BC;

    return mmc_wait_for_cmd(host, &cmd);
}

static int mmc_send_cxd(struct mmc_host* host, u32 arg, u32* cxd, int opcode)
{
    int err;
    struct mmc_command cmd = {};

    cmd.opcode = opcode;
    cmd.arg = arg;
    cmd.flags = MMC_RSP_R2 | MMC_CMD_AC;

    err = mmc_wait_for_cmd(host, &cmd);
    if (err) return err;

    memcpy(cxd, cmd.resp, sizeof(u32) * 4);
    return 0;
}

int mmc_send_cid(struct mmc_host* host, u32* cid)
{
    return mmc_send_cxd(host, 0, cid, MMC_ALL_SEND_CID);
}

int mmc_send_csd(struct mmc_card* card, u32* csd)
{
    return mmc_send_cxd(card->host, card->rca << 16, csd, MMC_SEND_CSD);
}

static int _mmc_select_card(struct mmc_host* host, struct mmc_card* card)
{
    struct mmc_command cmd = {};

    cmd.opcode = MMC_SELECT_CARD;

    if (card) {
        cmd.arg = card->rca << 16;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
    } else {
        cmd.arg = 0;
        cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
    }

    return mmc_wait_for_cmd(host, &cmd);
}

int mmc_select_card(struct mmc_card* card)
{
    return _mmc_select_card(card->host, card);
}

int mmc_deselect_card(struct mmc_host* host)
{
    return _mmc_select_card(host, NULL);
}

static int copyfrom(endpoint_t endpoint, const struct iovec_grant* iov,
                    vir_bytes offset, void* addr, size_t bytes)
{
    int ret;

    if (endpoint == SELF) {
        memcpy(addr, (void*)iov->iov_addr + offset, bytes);
    } else {
        if ((ret = safecopy_from(endpoint, iov->iov_grant, offset, addr,
                                 bytes)) != 0)
            return -ret;
    }

    return 0;
}

static int copyto(endpoint_t endpoint, const struct iovec_grant* iov,
                  vir_bytes offset, const void* addr, size_t bytes)
{
    int ret;

    if (endpoint == SELF) {
        memcpy((void*)iov->iov_addr + offset, addr, bytes);
    } else {
        if ((ret = safecopy_to(endpoint, iov->iov_grant, offset, addr,
                               bytes)) != 0)
            return -ret;
    }

    return 0;
}

static int mmc_blk_rw_single(struct mmc_card* card, int do_write,
                             unsigned int blknr, unsigned char* buf)
{
    struct mmc_request mrq = {};
    struct mmc_command cmd = {};
    struct mmc_data data = {};

    mrq.cmd = &cmd;
    mrq.data = &data;

    cmd.opcode = do_write ? MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
    cmd.arg = blknr;
    if (!(card->state & MMC_STATE_BLOCKADDR)) cmd.arg <<= 9;
    cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

    data.blksz = SECTOR_SIZE;
    data.blocks = 1;
    data.blk_addr = blknr;
    data.flags = do_write ? MMC_DATA_WRITE : MMC_DATA_READ;
    data.data = buf;
    data.len = SECTOR_SIZE;

    mmc_wait_for_req(card->host, &mrq);

    if (cmd.error) return cmd.error;
    if (data.error) return data.error;

    return 0;
}

static ssize_t mmc_blk_rdwt(dev_t minor, int do_write, loff_t pos,
                            endpoint_t endpoint, const struct iovec_grant* iov,
                            size_t count)
{
    static unsigned char copybuff[SECTOR_SIZE];
    const struct iovec_grant* ciov;
    struct mmc_blk_data* md;
    struct part_info* part;
    u64 part_end;
    size_t bytes_rdwt;
    int i, blk_size = SECTOR_SIZE;
    int err;

    if (count > NR_IOREQS) {
        return -EINVAL;
    }

    md = mmc_blk_get(minor);
    part = mmc_blk_part(minor);

    if (!md || !part) return -ENXIO;

    pos += part->base;

    if (pos & (SECTOR_SIZE - 1)) {
        return -EINVAL;
    }

    part_end = part->base + part->size;

    if (pos >= part_end) {
        return 0;
    }

    bytes_rdwt = 0;

    for (ciov = iov; ciov < iov + count; ciov++) {
        size_t io_size = ciov->iov_len;
        size_t io_offset = pos + bytes_rdwt;

        if (io_offset + io_size > part_end) io_size = part_end - io_offset;

        for (i = 0; i < io_size / blk_size; i++) {
            if (do_write) {
                err =
                    copyfrom(endpoint, ciov, i * blk_size, copybuff, blk_size);
                if (err) return err;

                err = mmc_blk_rw_single(md->card, do_write,
                                        (io_offset / blk_size) + i, copybuff);
                if (err) return err;

            } else {
                err = mmc_blk_rw_single(md->card, do_write,
                                        (io_offset / blk_size) + i, copybuff);
                if (err) return err;

                err = copyto(endpoint, ciov, i * blk_size, copybuff, blk_size);
                if (err) return err;
            }

            bytes_rdwt += blk_size;
        }
    }

    return bytes_rdwt;
}

static void mmc_blk_intr(unsigned mask)
{
    struct mmc_host* host;

    list_for_each_entry(host, &mmc_hosts, list)
    {
        if (mask & (1UL << host->irq)) host->ops->hw_intr(host);
    }
}

static void mmc_rescan(struct mmc_host* host)
{
    mmc_hw_reset_for_init(host);

    mmc_go_idle(host);

    mmc_attach_sd(host);
}

int mmc_add_host(struct mmc_host* host)
{
    struct device_info devinf;
    int ret;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "mmc%d", host->index);
    devinf.bus = NO_BUS_ID;
    devinf.class = mmc_class;
    devinf.parent = NO_DEVICE_ID;
    devinf.devt = NO_DEV;

    ret = dm_device_register(&devinf, &host->dev_id);
    if (ret) return -ret;

    list_add(&host->list, &mmc_hosts);
    mmc_rescan(host);

    return 0;
}

static void mmc_blk_register(struct mmc_blk_data* md)
{
    int i;
    struct device_info devinf;
    dev_t devt;
    device_id_t dev_id;

    /* register the device */
    memset(&devinf, 0, sizeof(devinf));
    devinf.bus = NO_BUS_ID;
    devinf.class = mmc_class;
    devinf.parent = md->card->dev_id;
    devinf.type = DT_BLOCKDEV;

    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (md->primary[i].size == 0) continue;

        devt = MAKE_DEV(DEV_MMC_BLK, md->devidx * NR_SUB_PER_DRIVE + i);
        dm_bdev_add(devt);

        snprintf(devinf.name, sizeof(devinf.name), "mmclk%dp%d", md->devidx, i);
        devinf.devt = devt;
        dm_device_register(&devinf, &dev_id);
    }

    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (md->logical[i].size == 0) continue;

        devt = MAKE_DEV(DEV_HD,
                        md->devidx * NR_SUB_PER_DRIVE + NR_PRIM_PER_DRIVE + i);
        dm_bdev_add(devt);

        snprintf(devinf.name, sizeof(devinf.name), "mmcblk%dp%d", md->devidx,
                 NR_PRIM_PER_DRIVE + i);
        devinf.devt = devt;
        dm_device_register(&devinf, &dev_id);
    }
}

static void print_mmc_blk_info(struct mmc_blk_data* md)
{
    int i;
    printl("mmcblk%d: mmc%d:%04x\n", md->devidx, md->card->host->index,
           md->card->rca);
    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (md->primary[i].size == 0) continue;

        u32 base_sect = md->primary[i].base >> SECTOR_SIZE_SHIFT;
        u32 nr_sects = md->primary[i].size >> SECTOR_SIZE_SHIFT;

        printl("mmcblk%d: %spartition %d: base %u(0x%x), size %u(0x%x) (in "
               "sectors)\n",
               md->devidx, i == 0 ? "  " : "     ", i, base_sect, base_sect,
               nr_sects, nr_sects);
    }
    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (md->logical[i].size == 0) continue;

        u32 base_sect = md->logical[i].base >> SECTOR_SIZE_SHIFT;
        u32 nr_sects = md->logical[i].size >> SECTOR_SIZE_SHIFT;

        printl("mmcblk%d:              "
               "%d: base %d(0x%x), size %d(0x%x) (in sectors)\n",
               md->devidx, i, base_sect, base_sect, nr_sects, nr_sects);
    }
}

static struct mmc_blk_data* mmc_blk_alloc_data(struct mmc_card* card)
{
    struct mmc_blk_data* md;
    int devidx;

    md = malloc(sizeof(*md));
    if (!md) goto out;

    memset(md, 0, sizeof(*md));
    md->card = card;

    devidx = idr_alloc(&mmc_blk_idr, md, 0, MAX_DEVICES);
    if (devidx < 0) goto out;

    md->devidx = devidx;

    return md;

out:
    free(md);
    return NULL;
}

static int mmc_blk_probe(struct mmc_card* card)
{
    struct mmc_blk_data* md;

    if (!(card->csd.cmdclass & CCC_BLOCK_READ)) return -ENODEV;

    md = mmc_blk_alloc_data(card);
    if (!md) return -ENOMEM;

    md->primary[0].size = card->csd.capacity << card->csd.read_blkbits;
    partition(&mmc_blk_driver, md->devidx * NR_SUB_PER_DRIVE, P_PRIMARY);
    print_mmc_blk_info(md);
    mmc_blk_register(md);

    return 0;
}

int mmc_add_card(struct mmc_card* card)
{
    struct device_info devinf;
    int ret;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "mmc%d:%04x", card->host->index,
             card->rca);
    devinf.bus = NO_BUS_ID;
    devinf.class = mmc_class;
    devinf.parent = card->host->dev_id;
    devinf.devt = NO_DEV;

    ret = dm_device_register(&devinf, &card->dev_id);
    if (ret) return -ret;

    mmc_blk_probe(card);

    return 0;
}

void mmc_remove_card(struct mmc_card* card) { free(card); }

static int mmc_init(void)
{
    struct sysinfo* sysinfo;
    int retval;

    printl(NAME ": MMC driver is running.\n");

    idr_init(&mmc_host_idr);
    idr_init(&mmc_blk_idr);

#if CONFIG_OF
    get_sysinfo(&sysinfo);
    boot_params = sysinfo->boot_params;
#endif

    retval = dm_class_register("mmc", &mmc_class);
    if (retval) return retval;

    return 0;
}

static void mmc_post_init(void)
{
#if CONFIG_MMC_SDHCI_IPROC
    sdhci_iproc_scan();
#endif

#if CONFIG_MMC_BCM2835
    bcm2835_scan();
#endif
}

int main(int argc, char** argv)
{
    serv_register_init_fresh_callback(mmc_init);
    serv_init();

    blockdriver_async_task(&mmc_blk_driver, MAX_THREADS, mmc_post_init);

    return 0;
}
