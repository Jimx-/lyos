#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <lyos/irqctl.h>

#include "mmc.h"
#include "mmchost.h"
#include "sdhci.h"

#define NAME "sdhci"

#define INIT_FREQ 400000

static int sdhci_data_line_cmd(struct mmc_command* cmd)
{
    return cmd->data || cmd->flags & MMC_RSP_BUSY;
}

static void sdhci_set_transfer_irqs(struct sdhci_host* host)
{
    u32 pio_irqs = SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL;
    u32 dma_irqs = SDHCI_INT_DMA_END | SDHCI_INT_ADMA_ERROR;

    host->ier = (host->ier & ~dma_irqs) | pio_irqs;

    sdhci_writel(host, SDHCI_INT_ENABLE, host->ier);
    sdhci_writel(host, SDHCI_SIGNAL_ENABLE, host->ier);
}

static void sdhci_set_transfer_mode(struct sdhci_host* host,
                                    struct mmc_command* cmd)
{
    struct mmc_data* data = cmd->data;
    u16 mode = 0;

    if (!data) {
        mode = sdhci_readw(host, SDHCI_TRANSFER_MODE);
        sdhci_writew(host, SDHCI_TRANSFER_MODE,
                     mode & ~(SDHCI_TRNS_AUTO_CMD12 | SDHCI_TRNS_AUTO_CMD23));
        return;
    }

    if (data->flags & MMC_DATA_READ) mode |= SDHCI_TRNS_READ;

    sdhci_writew(host, SDHCI_TRANSFER_MODE, mode);
}

static void sdhci_set_block_info(struct sdhci_host* host, struct mmc_data* data)
{
    sdhci_writew(host, SDHCI_BLOCK_SIZE, data->blksz & 0xFFF);
    sdhci_writew(host, SDHCI_BLOCK_COUNT, data->blocks);
}

u16 sdhci_calc_clk(struct sdhci_host* host, unsigned int clock)
{
    int div = 0;
    u16 clk = 0;

    if (host->version >= SDHCI_SPEC_300) {
        if (host->max_clk <= clock)
            div = 1;
        else {
            for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) {
                if ((host->max_clk / div) <= clock) break;
            }
            div >>= 1;
        }
    }

    clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
    clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
           << SDHCI_DIVIDER_HI_SHIFT;

    return clk;
}

void sdhci_enable_clk(struct sdhci_host* host, u16 clk)
{
    clk |= SDHCI_CLOCK_INT_EN;
    sdhci_writew(host, SDHCI_CLOCK_CONTROL, clk);

    while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL)) &
             SDHCI_CLOCK_INT_STABLE)) {
        usleep(10);
    }

    clk |= SDHCI_CLOCK_CARD_EN;
    sdhci_writew(host, SDHCI_CLOCK_CONTROL, clk);
}

void sdhci_set_clock(struct sdhci_host* host, unsigned int clock)
{
    u16 clk;

    clk = sdhci_calc_clk(host, clock);
    sdhci_enable_clk(host, clk);
}

void sdhci_set_bus_width(struct sdhci_host* host, int width)
{
    u8 ctrl;

    ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
    if (width == MMC_BUS_WIDTH_8) {
        ctrl &= ~SDHCI_CTRL_4BITBUS;
        ctrl |= SDHCI_CTRL_8BITBUS;
    } else {
        ctrl &= ~SDHCI_CTRL_8BITBUS;

        if (width == MMC_BUS_WIDTH_4)
            ctrl |= SDHCI_CTRL_4BITBUS;
        else
            ctrl &= ~SDHCI_CTRL_4BITBUS;
    }
    sdhci_writeb(host, SDHCI_HOST_CONTROL, ctrl);
}

void sdhci_reset(struct sdhci_host* host, u8 mask)
{
    sdhci_writeb(host, SDHCI_SOFTWARE_RESET, mask);

    while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask)
        usleep(10);
}

static int sdhci_send_command(struct sdhci_host* host, struct mmc_command* cmd)
{
    u32 mask;
    int flags;

    cmd->error = 0;

    mask = SDHCI_CMD_INHIBIT;
    if (sdhci_data_line_cmd(cmd)) mask |= SDHCI_DATA_INHIBIT;

    if (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) return FALSE;

    host->cmd = cmd;
    if (sdhci_data_line_cmd(cmd)) host->data_cmd = cmd;

    if (cmd->data) {
        host->data = cmd->data;
        host->blocks = cmd->data->blocks;
        host->data->bytes_xfered = 0;

        sdhci_set_transfer_irqs(host);
        sdhci_set_block_info(host, cmd->data);
    }

    sdhci_writel(host, SDHCI_ARGUMENT, cmd->arg);

    sdhci_set_transfer_mode(host, cmd);

    if (!(cmd->flags & MMC_RSP_PRESENT))
        flags = SDHCI_CMD_RESP_NONE;
    else if (cmd->flags & MMC_RSP_136)
        flags = SDHCI_CMD_RESP_LONG;
    else if (cmd->flags & MMC_RSP_BUSY)
        flags = SDHCI_CMD_RESP_SHORT_BUSY;
    else
        flags = SDHCI_CMD_RESP_SHORT;

    if (cmd->flags & MMC_RSP_CRC) flags |= SDHCI_CMD_CRC;
    if (cmd->flags & MMC_RSP_OPCODE) flags |= SDHCI_CMD_INDEX;

    if (cmd->data) flags |= SDHCI_CMD_DATA;

    sdhci_writew(host, SDHCI_COMMAND, SDHCI_MAKE_CMD(cmd->opcode, flags));

    return TRUE;
}

void sdhci_set_ios(struct mmc_host* mmc, struct mmc_ios* ios)
{
    struct sdhci_host* host = mmc_priv(mmc);

    if (!ios->clock || ios->clock != host->clock) {
        host->ops->set_clock(host, ios->clock);
        host->clock = ios->clock;
    }

    host->ops->set_bus_width(host, ios->bus_width);
}

static void sdhci_request(struct mmc_host* mmc, struct mmc_request* mrq)
{
    struct sdhci_host* host = mmc_priv(mmc);
    struct mmc_command* cmd = mrq->cmd;

    sdhci_send_command(host, cmd);
}

static void sdhci_read_rsp_136(struct sdhci_host* host, struct mmc_command* cmd)
{
    int i, reg;

    for (i = 0; i < 4; i++) {
        reg = SDHCI_RESPONSE + (3 - i) * 4;
        cmd->resp[i] = sdhci_readl(host, reg);
    }

    for (i = 0; i < 4; i++) {
        cmd->resp[i] <<= 8;
        if (i != 3) cmd->resp[i] |= cmd->resp[i + 1] >> 24;
    }
}

static void sdhci_finish_command(struct sdhci_host* host)
{
    struct mmc_command* cmd = host->cmd;

    host->cmd = NULL;

    if (cmd->flags & MMC_RSP_PRESENT) {
        if (cmd->flags & MMC_RSP_136) {
            sdhci_read_rsp_136(host, cmd);
        } else {
            cmd->resp[0] = sdhci_readl(host, SDHCI_RESPONSE);
        }
    }

    if (!cmd->data) mmc_request_done(host->mmc, cmd->mrq);
}

static void sdhci_cmd_intr(struct sdhci_host* host, u32 intmask, u32* intmaskp)
{
    if (!host->cmd) return;

    if (intmask & SDHCI_INT_RESPONSE) sdhci_finish_command(host);
}

static void sdhci_read_block_pio(struct sdhci_host* host)
{
    size_t blksize, len, chunk;
    u8* buf;
    u32 val;

    blksize = host->data->blksz;
    chunk = 0;

    while (blksize) {
        len = min(blksize, host->data->len);

        blksize -= len;
        host->data->len -= len;
        buf = host->data->data;

        while (len) {
            if (chunk == 0) {
                val = sdhci_readl(host, SDHCI_BUFFER);
                chunk = 4;
            }

            *buf = val & 0xFF;

            buf++;
            val >>= 8;
            chunk--;
            len--;
        }

        host->data->data = buf;
    }
}

static void sdhci_write_block_pio(struct sdhci_host* host)
{
    size_t blksize, len, chunk;
    u8* buf;
    u32 val;

    blksize = host->data->blksz;
    chunk = 0;
    val = 0;

    while (blksize) {
        len = min(blksize, host->data->len);

        blksize -= len;
        host->data->len -= len;
        buf = host->data->data;

        while (len) {
            val |= (u32)*buf << (chunk * 8);

            buf++;
            chunk++;
            len--;

            if ((chunk == 4) || ((len == 0) && (blksize == 0))) {
                sdhci_writel(host, SDHCI_BUFFER, val);
                chunk = 0;
                val = 0;
            }
        }

        host->data->data = buf;
    }
}

static void sdhci_transfer_pio(struct sdhci_host* host)
{
    u32 mask;

    if (host->blocks == 0) return;

    if (host->data->flags & MMC_DATA_READ)
        mask = SDHCI_DATA_AVAILABLE;
    else
        mask = SDHCI_SPACE_AVAILABLE;

    while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
        if (host->data->flags & MMC_DATA_READ)
            sdhci_read_block_pio(host);
        else
            sdhci_write_block_pio(host);

        host->blocks--;
        if (!host->blocks) break;
    }
}

static void sdhci_finish_data(struct sdhci_host* host)
{
    struct mmc_command* data_cmd = host->data_cmd;
    struct mmc_data* data = host->data;

    host->data = NULL;
    host->data_cmd = NULL;

    if (data->error) {
        if (!host->cmd || host->cmd == data_cmd)
            sdhci_reset(host, SDHCI_RESET_CMD);
        sdhci_reset(host, SDHCI_RESET_DATA);
    }

    if (data->error)
        data->bytes_xfered = 0;
    else
        data->bytes_xfered = data->blksz * data->blocks;

    mmc_request_done(host->mmc, data->mrq);
}

static void sdhci_data_intr(struct sdhci_host* host, u32 intmask)
{
    if (intmask & SDHCI_INT_DATA_TIMEOUT) host->data->error = -ETIMEDOUT;
    if (intmask & SDHCI_INT_DATA_END_BIT) host->data->error = -EILSEQ;

    if (host->data->error)
        sdhci_finish_data(host);
    else {
        if (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL))
            sdhci_transfer_pio(host);

        if (intmask & SDHCI_INT_DATA_END) sdhci_finish_data(host);
    }
}

static void sdhci_intr(struct mmc_host* mmc)
{
    struct sdhci_host* host = mmc_priv(mmc);
    u32 intmask, mask;
    int max_loop = 16;

    intmask = sdhci_readl(host, SDHCI_INT_STATUS);
    if (!intmask || intmask == 0xffffffff) {
        return;
    }

    do {
        mask = intmask &
               (SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK | SDHCI_INT_BUS_POWER);
        sdhci_writel(host, SDHCI_INT_STATUS, mask);

        if (intmask & SDHCI_INT_CMD_MASK)
            sdhci_cmd_intr(host, intmask & SDHCI_INT_CMD_MASK, &intmask);

        if (intmask & SDHCI_INT_DATA_MASK)
            sdhci_data_intr(host, intmask & SDHCI_INT_DATA_MASK);

        intmask &=
            ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE |
              SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK | SDHCI_INT_ERROR |
              SDHCI_INT_BUS_POWER | SDHCI_INT_RETUNE | SDHCI_INT_CARD_INT);

        if (intmask) sdhci_writel(host, SDHCI_INT_STATUS, mask);

        intmask = sdhci_readl(host, SDHCI_INT_STATUS);
    } while (intmask && --max_loop);

    irq_enable(&host->irq_hook);
}

static const struct mmc_host_ops sdhci_mmc_host_ops = {
    .request = sdhci_request,
    .set_ios = sdhci_set_ios,
    .hw_intr = sdhci_intr,
};

struct sdhci_host* sdhci_alloc_host(size_t priv_size)
{
    struct mmc_host* mmc;
    struct sdhci_host* host;

    mmc = mmc_alloc_host(sizeof(struct sdhci_host) + priv_size);
    if (!mmc) return NULL;

    host = mmc_priv(mmc);
    host->mmc = mmc;
    mmc->ops = &sdhci_mmc_host_ops;

    return host;
}

int sdhci_add_host(struct sdhci_host* host)
{
    struct mmc_host* mmc = host->mmc;
    unsigned int ocr_avail;
    u16 v;
    int ret;

    v = sdhci_readw(host, SDHCI_HOST_VERSION);
    host->version = (v & SDHCI_SPEC_VER_MASK) >> SDHCI_SPEC_VER_SHIFT;

    host->caps = sdhci_readl(host, SDHCI_CAPABILITIES);

    if (host->version >= SDHCI_SPEC_300)
        host->caps1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);

    printl("Version:   0x%08x | Present:  0x%08x\n", v,
           sdhci_readl(host, SDHCI_PRESENT_STATE));
    printl("Caps:      0x%08x | Caps_1:   0x%08x\n", host->caps, host->caps1);

    if (host->version >= SDHCI_SPEC_300)
        host->max_clk =
            (host->caps & SDHCI_CLOCK_V3_BASE_MASK) >> SDHCI_CLOCK_BASE_SHIFT;
    else
        host->max_clk =
            (host->caps & SDHCI_CLOCK_BASE_MASK) >> SDHCI_CLOCK_BASE_SHIFT;

    host->max_clk *= 1000000;

    host->clk_mul =
        (host->caps1 & SDHCI_CLOCK_MUL_MASK) >> SDHCI_CLOCK_MUL_SHIFT;
    if (host->clk_mul) host->clk_mul++;

    ocr_avail = 0;

    if (host->caps & SDHCI_CAN_VDD_330)
        ocr_avail |= MMC_VDD_32_33 | MMC_VDD_33_34;
    if (host->caps & SDHCI_CAN_VDD_300)
        ocr_avail |= MMC_VDD_29_30 | MMC_VDD_30_31;
    if (host->caps & SDHCI_CAN_VDD_180) ocr_avail |= MMC_VDD_165_195;

    mmc->ocr_avail = ocr_avail;

    sdhci_reset(host, SDHCI_RESET_ALL);

    sdhci_writeb(host, SDHCI_POWER_CONTROL, SDHCI_POWER_330 | SDHCI_POWER_ON);
    while (!(sdhci_readb(host, SDHCI_POWER_CONTROL) & SDHCI_POWER_ON))
        usleep(10);

    host->ops->set_clock(host, INIT_FREQ);
    host->clock = INIT_FREQ;

    host->ier = SDHCI_INT_BUS_POWER | SDHCI_INT_DATA_END_BIT |
                SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_INDEX |
                SDHCI_INT_END_BIT | SDHCI_INT_CRC | SDHCI_INT_TIMEOUT |
                SDHCI_INT_DATA_END | SDHCI_INT_RESPONSE;

    sdhci_writel(host, SDHCI_INT_ENABLE, host->ier);
    sdhci_writel(host, SDHCI_SIGNAL_ENABLE, host->ier);

    sdhci_writeb(host, SDHCI_TIMEOUT_CONTROL, 0xe);

    host->irq_hook = host->irq;
    ret = irq_setpolicy(host->irq, 0, &host->irq_hook);
    if (ret != 0) return -ret;

    mmc->irq = host->irq;

    return mmc_add_host(mmc);
}
