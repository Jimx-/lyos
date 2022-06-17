#include <lyos/const.h>
#include <stddef.h>
#include <unistd.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <lyos/irqctl.h>
#include <errno.h>
#include <asm/io.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "mmc.h"
#include "mmchost.h"

#define SDCMD  0x00 /* Command to SD card              - 16 R/W */
#define SDARG  0x04 /* Argument to SD card             - 32 R/W */
#define SDTOUT 0x08 /* Start value for timeout counter - 32 R/W */
#define SDCDIV 0x0c /* Start value for clock divider   - 11 R/W */
#define SDRSP0 0x10 /* SD card response (31:0)         - 32 R   */
#define SDRSP1 0x14 /* SD card response (63:32)        - 32 R   */
#define SDRSP2 0x18 /* SD card response (95:64)        - 32 R   */
#define SDRSP3 0x1c /* SD card response (127:96)       - 32 R   */
#define SDHSTS 0x20 /* SD host status                  - 11 R/W */
#define SDVDD  0x30 /* SD card power control           -  1 R/W */
#define SDEDM  0x34 /* Emergency Debug Mode            - 13 R/W */
#define SDHCFG 0x38 /* Host configuration              -  2 R/W */
#define SDHBCT 0x3c /* Host byte count (debug)         - 32 R/W */
#define SDDATA 0x40 /* Data to/from SD card            - 32 R/W */
#define SDHBLC 0x50 /* Host block count (SDIO/SDHC)    -  9 R/W */

#define SDCMD_NEW_FLAG      0x8000
#define SDCMD_FAIL_FLAG     0x4000
#define SDCMD_BUSYWAIT      0x800
#define SDCMD_NO_RESPONSE   0x400
#define SDCMD_LONG_RESPONSE 0x200
#define SDCMD_WRITE_CMD     0x80
#define SDCMD_READ_CMD      0x40
#define SDCMD_CMD_MASK      0x3f

#define SDCDIV_MAX_CDIV 0x7ff

#define SDHSTS_BUSY_IRPT    0x400
#define SDHSTS_BLOCK_IRPT   0x200
#define SDHSTS_SDIO_IRPT    0x100
#define SDHSTS_REW_TIME_OUT 0x80
#define SDHSTS_CMD_TIME_OUT 0x40
#define SDHSTS_CRC16_ERROR  0x20
#define SDHSTS_CRC7_ERROR   0x10
#define SDHSTS_FIFO_ERROR   0x08
/* Reserved */
/* Reserved */
#define SDHSTS_DATA_FLAG 0x01

#define SDHSTS_TRANSFER_ERROR_MASK                                  \
    (SDHSTS_CRC7_ERROR | SDHSTS_CRC16_ERROR | SDHSTS_REW_TIME_OUT | \
     SDHSTS_FIFO_ERROR)

#define SDHSTS_ERROR_MASK (SDHSTS_CMD_TIME_OUT | SDHSTS_TRANSFER_ERROR_MASK)

#define SDHCFG_BUSY_IRPT_EN  BIT(10)
#define SDHCFG_BLOCK_IRPT_EN BIT(8)
#define SDHCFG_SDIO_IRPT_EN  BIT(5)
#define SDHCFG_DATA_IRPT_EN  BIT(4)
#define SDHCFG_SLOW_CARD     BIT(3)
#define SDHCFG_WIDE_EXT_BUS  BIT(2)
#define SDHCFG_WIDE_INT_BUS  BIT(1)
#define SDHCFG_REL_CMD_LINE  BIT(0)

#define SDVDD_POWER_OFF 0
#define SDVDD_POWER_ON  1

#define SDEDM_FORCE_DATA_MODE BIT(19)
#define SDEDM_CLOCK_PULSE     BIT(20)
#define SDEDM_BYPASS          BIT(21)

#define SDEDM_WRITE_THRESHOLD_SHIFT 9
#define SDEDM_READ_THRESHOLD_SHIFT  14
#define SDEDM_THRESHOLD_MASK        0x1f

#define SDEDM_FSM_MASK         0xf
#define SDEDM_FSM_IDENTMODE    0x0
#define SDEDM_FSM_DATAMODE     0x1
#define SDEDM_FSM_READDATA     0x2
#define SDEDM_FSM_WRITEDATA    0x3
#define SDEDM_FSM_READWAIT     0x4
#define SDEDM_FSM_READCRC      0x5
#define SDEDM_FSM_WRITECRC     0x6
#define SDEDM_FSM_WRITEWAIT1   0x7
#define SDEDM_FSM_POWERDOWN    0x8
#define SDEDM_FSM_POWERUP      0x9
#define SDEDM_FSM_WRITESTART1  0xa
#define SDEDM_FSM_WRITESTART2  0xb
#define SDEDM_FSM_GENPULSES    0xc
#define SDEDM_FSM_WRITEWAIT2   0xd
#define SDEDM_FSM_STARTPOWDOWN 0xf

#define SDDATA_FIFO_WORDS 16

#define FIFO_READ_THRESHOLD   4
#define FIFO_WRITE_THRESHOLD  4
#define SDDATA_FIFO_PIO_BURST 8

struct bcm2835_host {
    void* ioaddr;

    int clock;

    int irq;
    int irq_hook;

    u32 hcfg;
    u32 cdiv;

    struct mmc_request* mrq;
    struct mmc_command* cmd;
    struct mmc_data* data;
    int data_complete : 1, use_busy : 1;

    unsigned int blocks;
};

extern void* boot_params;

static const char* const bcm2835_compat[] = {"brcm,bcm2835-sdhost", NULL};

static void bcm2835_reset_internal(struct bcm2835_host* host)
{
    u32 temp;

    writel(host->ioaddr + SDVDD, SDVDD_POWER_OFF);
    writel(host->ioaddr + SDCMD, 0);
    writel(host->ioaddr + SDARG, 0);
    writel(host->ioaddr + SDTOUT, 0xf00000);
    /* writel(host->ioaddr + SDCDIV, 0); */
    writel(host->ioaddr + SDHSTS, 0x7f8);
    writel(host->ioaddr + SDHCFG, 0);
    writel(host->ioaddr + SDHBCT, 0);
    writel(host->ioaddr + SDHBLC, 0);

    temp = readl(host->ioaddr + SDEDM);
    temp &= ~((SDEDM_THRESHOLD_MASK << SDEDM_READ_THRESHOLD_SHIFT) |
              (SDEDM_THRESHOLD_MASK << SDEDM_WRITE_THRESHOLD_SHIFT));
    temp |= (FIFO_READ_THRESHOLD << SDEDM_READ_THRESHOLD_SHIFT) |
            (FIFO_WRITE_THRESHOLD << SDEDM_WRITE_THRESHOLD_SHIFT);
    writel(host->ioaddr + SDEDM, temp);

    usleep(20000);
    writel(host->ioaddr + SDVDD, SDVDD_POWER_ON);
    usleep(20000);

    host->clock = 0;

    writel(host->ioaddr + SDHCFG, host->hcfg);
    /* writel(host->ioaddr + SDCDIV, host->cdiv); */
}

static void bcm2835_finish_request(struct bcm2835_host* host)
{
    struct mmc_request* mrq;

    mrq = host->mrq;
    host->mrq = NULL;
    host->cmd = NULL;
    host->data = NULL;

    mmc_request_done(mmc_from_priv(host), mrq);
}

static u32 bcm2835_read_wait_sdcmd(struct bcm2835_host* host, u32 max_ms)
{
    u32 value;

    while ((value = readl(host->ioaddr + SDCMD)) & SDCMD_NEW_FLAG)
        ;

    return value;
}

static void bcm2835_set_transfer_irqs(struct bcm2835_host* host)
{
    u32 all_irqs =
        SDHCFG_DATA_IRPT_EN | SDHCFG_BLOCK_IRPT_EN | SDHCFG_BUSY_IRPT_EN;

    host->hcfg =
        (host->hcfg & ~all_irqs) | SDHCFG_DATA_IRPT_EN | SDHCFG_BUSY_IRPT_EN;

    writel(host->ioaddr + SDHCFG, host->hcfg);
}

static void bcm2835_prepare_data(struct bcm2835_host* host,
                                 struct mmc_command* cmd)
{
    struct mmc_data* data = cmd->data;

    host->data = data;
    if (!data) return;

    host->data_complete = FALSE;
    host->data->bytes_xfered = 0;

    host->blocks = data->blocks;

    bcm2835_set_transfer_irqs(host);

    writel(host->ioaddr + SDHBCT, data->blksz);
    writel(host->ioaddr + SDHBLC, data->blocks);
}

static int bcm2835_send_command(struct bcm2835_host* host,
                                struct mmc_command* cmd)
{
    u32 sdcmd, sdhsts;

    sdcmd = bcm2835_read_wait_sdcmd(host, 100);
    if (sdcmd & SDCMD_NEW_FLAG) {
        cmd->error = -EILSEQ;
        bcm2835_finish_request(host);
        return FALSE;
    }

    host->cmd = cmd;

    sdhsts = readl(host->ioaddr + SDHSTS);
    if (sdhsts & SDHSTS_ERROR_MASK) writel(host->ioaddr + SDHSTS, sdhsts);

    if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
        cmd->error = -EINVAL;
        bcm2835_finish_request(host);
        return FALSE;
    }

    bcm2835_prepare_data(host, cmd);

    writel(host->ioaddr + SDARG, cmd->arg);

    sdcmd = cmd->opcode & SDCMD_CMD_MASK;

    host->use_busy = FALSE;
    if (!(cmd->flags & MMC_RSP_PRESENT)) {
        sdcmd |= SDCMD_NO_RESPONSE;
    } else {
        if (cmd->flags & MMC_RSP_136) sdcmd |= SDCMD_LONG_RESPONSE;
        if (cmd->flags & MMC_RSP_BUSY) {
            sdcmd |= SDCMD_BUSYWAIT;
            host->use_busy = TRUE;
        }
    }

    if (cmd->data) {
        if (cmd->data->flags & MMC_DATA_WRITE) sdcmd |= SDCMD_WRITE_CMD;
        if (cmd->data->flags & MMC_DATA_READ) sdcmd |= SDCMD_READ_CMD;
    }

    writel(host->ioaddr + SDCMD, sdcmd | SDCMD_NEW_FLAG);

    return TRUE;
}

static void bcm2835_transfer_complete(struct bcm2835_host* host)
{
    struct mmc_data* data;

    data = host->data;
    host->data = NULL;

    bcm2835_finish_request(host);
}

static void bcm2835_finish_data(struct bcm2835_host* host)
{
    struct mmc_data* data;

    data = host->data;

    host->hcfg &= ~(SDHCFG_DATA_IRPT_EN | SDHCFG_BLOCK_IRPT_EN);
    writel(host->ioaddr + SDHCFG, host->hcfg);

    data->bytes_xfered = data->error ? 0 : (data->blksz * data->blocks);

    host->data_complete = TRUE;

    if (!host->cmd) {
        bcm2835_transfer_complete(host);
    }
}

static void bcm2835_finish_command(struct bcm2835_host* host)
{
    struct mmc_command* cmd = host->cmd;
    u32 sdcmd;

    sdcmd = bcm2835_read_wait_sdcmd(host, 100);
    if (sdcmd & SDCMD_NEW_FLAG) {
        host->cmd->error = -EIO;
        bcm2835_finish_request(host);
    } else if (sdcmd & SDCMD_FAIL_FLAG) {
        u32 sdhsts = readl(host->ioaddr + SDHSTS);
    }

    u32 sdhsts = readl(host->ioaddr + SDHSTS);

    if (cmd->flags & MMC_RSP_PRESENT) {
        if (cmd->flags & MMC_RSP_136) {
            int i;

            for (i = 0; i < 4; i++) {
                cmd->resp[3 - i] = readl(host->ioaddr + SDRSP0 + i * 4);
            }
        } else {
            cmd->resp[0] = readl(host->ioaddr + SDRSP0);
        }
    }

    host->cmd = NULL;
    if (!host->data)
        bcm2835_finish_request(host);
    else if (host->data_complete)
        bcm2835_transfer_complete(host);
}

static void bcm2835_request(struct mmc_host* mmc, struct mmc_request* mrq)
{
    struct bcm2835_host* host = mmc_priv(mmc);

    if (mrq->cmd) mrq->cmd->error = 0;
    if (mrq->data) mrq->data->error = 0;

    host->mrq = mrq;

    if (mrq->cmd && bcm2835_send_command(host, mrq->cmd)) {
        if (!host->use_busy) bcm2835_finish_command(host);
    }
}

void bcm2835_set_ios(struct mmc_host* mmc, struct mmc_ios* ios)
{
    struct bcm2835_host* host = mmc_priv(mmc);

    host->hcfg &= ~SDHCFG_WIDE_EXT_BUS;
    if (ios->bus_width == MMC_BUS_WIDTH_4) host->hcfg |= SDHCFG_WIDE_EXT_BUS;

    host->hcfg |= SDHCFG_WIDE_INT_BUS;

    host->hcfg |= SDHCFG_SLOW_CARD;

    writel(host->ioaddr + SDHCFG, host->hcfg);
}

static void bcm2835_check_data_error(struct bcm2835_host* host, u32 intmask)
{
    if (!host->data) return;
    if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR))
        host->data->error = -EILSEQ;
    if (intmask & SDHSTS_REW_TIME_OUT) host->data->error = -ETIMEDOUT;
}

static void bcm2835_transfer_block_pio(struct bcm2835_host* host, int is_read)
{
    size_t blksize;

    blksize = host->data->blksz;

    while (blksize) {
        int copy_words;
        size_t len;
        u32* buf;

        len = min(blksize, host->data->len);
        if (len % 4) {
            host->data->error = -EINVAL;
            break;
        }

        blksize -= len;
        host->data->len -= len;
        buf = host->data->data;

        copy_words = len / 4;

        while (copy_words) {
            int burst_words, words;
            u32 edm;

            burst_words = min(SDDATA_FIFO_PIO_BURST, copy_words);
            edm = readl(host->ioaddr + SDEDM);
            if (is_read)
                words = ((edm >> 4) & 0x1f);
            else
                words = SDDATA_FIFO_WORDS - ((edm >> 4) & 0x1f);

            if (words < burst_words)
                continue;
            else if (words > copy_words)
                words = copy_words;

            copy_words -= words;

            while (words) {
                if (is_read) {
                    *(buf++) = readl(host->ioaddr + SDDATA);
                } else
                    writel(host->ioaddr + SDDATA, *(buf++));
                words--;
            }

            host->data->data = buf;
        }
    }
}

static void bcm2835_transfer_pio(struct bcm2835_host* host)
{
    u32 sdhsts;
    int is_read;

    is_read = (host->data->flags & MMC_DATA_READ) != 0;
    bcm2835_transfer_block_pio(host, is_read);

    sdhsts = readl(host->ioaddr + SDHSTS);
    if (sdhsts & (SDHSTS_CRC16_ERROR | SDHSTS_CRC7_ERROR | SDHSTS_FIFO_ERROR))
        host->data->error = -EILSEQ;
    else if ((sdhsts & (SDHSTS_CMD_TIME_OUT | SDHSTS_REW_TIME_OUT)))
        host->data->error = -ETIMEDOUT;
}

static void bcm2835_data_irq(struct bcm2835_host* host, u32 intmask)
{
    if (!host->data) return;

    bcm2835_check_data_error(host, intmask);
    if (host->data->error) goto finished;

    if (host->data->flags & MMC_DATA_WRITE) {
        host->hcfg &= ~(SDHCFG_DATA_IRPT_EN);
        host->hcfg |= SDHCFG_BLOCK_IRPT_EN;
        writel(host->ioaddr + SDHCFG, host->hcfg);
        bcm2835_transfer_pio(host);
    } else {
        bcm2835_transfer_pio(host);
        host->blocks--;
        if ((host->blocks == 0) || host->data->error) goto finished;
    }
    return;

finished:
    host->hcfg &= ~(SDHCFG_DATA_IRPT_EN | SDHCFG_BLOCK_IRPT_EN);
    writel(host->ioaddr + SDHCFG, host->hcfg);
}

static void bcm2835_block_irq(struct bcm2835_host* host)
{
    if (host->data->error || (--host->blocks == 0))
        bcm2835_finish_data(host);
    else
        bcm2835_transfer_pio(host);
}

static void bcm2835_intr(struct mmc_host* mmc)
{
    struct bcm2835_host* host = mmc_priv(mmc);
    u32 intmask;

    intmask = readl(host->ioaddr + SDHSTS);

    writel(host->ioaddr + SDHSTS, SDHSTS_BUSY_IRPT | SDHSTS_BLOCK_IRPT |
                                      SDHSTS_SDIO_IRPT | SDHSTS_DATA_FLAG);

    if (intmask & SDHSTS_BLOCK_IRPT) {
        bcm2835_check_data_error(host, intmask);
        bcm2835_block_irq(host);
    }

    if ((intmask & SDHSTS_DATA_FLAG) && (host->hcfg & SDHCFG_DATA_IRPT_EN)) {
        bcm2835_data_irq(host, intmask);
        if (host->data) {
            if (host->blocks == 0 || host->data->error)
                bcm2835_finish_data(host);
        }
    }

    irq_enable(&host->irq_hook);
}

static void bcm2835_reset(struct mmc_host* mmc)
{
    struct bcm2835_host* host = mmc_priv(mmc);

    bcm2835_reset_internal(host);
}

static const struct mmc_host_ops bcm2835_mmc_host_ops = {
    .request = bcm2835_request,
    .set_ios = bcm2835_set_ios,
    .hw_intr = bcm2835_intr,
    .hw_reset = bcm2835_reset,
};

static int bcm2835_add_host(struct bcm2835_host* host)
{
    struct mmc_host* mmc = mmc_from_priv(host);
    int ret;

    mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

    host->hcfg = SDHCFG_BUSY_IRPT_EN | SDHCFG_WIDE_INT_BUS;
    bcm2835_reset_internal(host);

    host->irq_hook = host->irq;
    ret = irq_setpolicy(host->irq, 0, &host->irq_hook);
    if (ret != 0) return -ret;

    ret = irq_enable(&host->irq_hook);
    if (ret != 0) return -ret;

    mmc->irq = host->irq;

    return mmc_add_host(mmc);
}

static int fdt_scan_bcm2835(void* blob, unsigned long offset, const char* name,
                            int depth, void* arg)
{
    struct mmc_host* mmc;
    struct bcm2835_host* host;
    phys_bytes base, size;
    void* reg_base;
    int irq;
    int ret;

    if (!of_flat_dt_match(blob, offset, bcm2835_compat)) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    reg_base = mm_map_phys(SELF, base, size, MMP_IO);
    if (reg_base == MAP_FAILED) return 0;

    irq = irq_of_parse_and_map(blob, offset, 0);
    if (!irq) return 0;

    mmc = mmc_alloc_host(sizeof(struct bcm2835_host));
    if (!mmc) return 0;

    host = mmc_priv(mmc);
    mmc->ops = &bcm2835_mmc_host_ops;

    host->ioaddr = reg_base;
    host->irq = irq;

    bcm2835_add_host(host);

    return 1;
}

void bcm2835_scan(void) { of_scan_fdt(fdt_scan_bcm2835, NULL, boot_params); }
