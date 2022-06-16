#include <lyos/const.h>
#include <stddef.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "sdhci.h"

struct sdhci_iproc_host {
    u32 shadow_cmd;
    u32 shadow_blk;
    int is_cmd_shadowed;
    int is_blk_shadowed;
};

extern void* boot_params;

static const char* const bcm2835_compat[] = {"brcm,bcm2835-sdhci", NULL};

#define REG_OFFSET_IN_BITS(reg) ((reg) << 3 & 0x18)

static u32 sdhci_iproc_readl(struct sdhci_host* host, int reg)
{
    return readl(host->ioaddr + reg);
}

static u16 sdhci_iproc_readw(struct sdhci_host* host, int reg)
{
    struct sdhci_iproc_host* iproc_host = sdhci_priv(host);
    u32 val;

    if ((reg == SDHCI_TRANSFER_MODE) && iproc_host->is_cmd_shadowed) {
        val = iproc_host->shadow_cmd;
    } else if ((reg == SDHCI_BLOCK_SIZE || reg == SDHCI_BLOCK_COUNT) &&
               iproc_host->is_blk_shadowed) {
        val = iproc_host->shadow_blk;
    } else {
        val = sdhci_iproc_readl(host, reg & ~3);
    }

    return val >> REG_OFFSET_IN_BITS(reg) & 0xffff;
}

static u8 sdhci_iproc_readb(struct sdhci_host* host, int reg)
{
    u32 val = sdhci_iproc_readl(host, reg & ~3);
    return val >> REG_OFFSET_IN_BITS(reg) & 0xff;
}

static void sdhci_iproc_writel(struct sdhci_host* host, int reg, u32 val)
{
    writel(host->ioaddr + reg, val);
}

static void sdhci_iproc_writew(struct sdhci_host* host, int reg, u16 val)
{
    struct sdhci_iproc_host* iproc_host = sdhci_priv(host);
    u32 word_shift = REG_OFFSET_IN_BITS(reg);
    u32 mask = 0xffff << word_shift;
    u32 oldval, newval;

    if (reg == SDHCI_COMMAND) {
        if (iproc_host->is_blk_shadowed) {
            sdhci_iproc_writel(host, SDHCI_BLOCK_SIZE, iproc_host->shadow_blk);
            iproc_host->is_blk_shadowed = FALSE;
        }
        oldval = iproc_host->shadow_cmd;
        iproc_host->is_cmd_shadowed = FALSE;
    } else if ((reg == SDHCI_BLOCK_SIZE || reg == SDHCI_BLOCK_COUNT) &&
               iproc_host->is_blk_shadowed) {
        oldval = iproc_host->shadow_blk;
    } else {
        oldval = sdhci_readl(host, reg);
    }

    newval = (oldval & ~mask) | (val << word_shift);

    if (reg == SDHCI_TRANSFER_MODE) {
        iproc_host->shadow_cmd = newval;
        iproc_host->is_cmd_shadowed = TRUE;
    } else if (reg == SDHCI_BLOCK_SIZE || reg == SDHCI_BLOCK_COUNT) {
        iproc_host->shadow_blk = newval;
        iproc_host->is_blk_shadowed = TRUE;
    } else {
        sdhci_iproc_writel(host, reg & ~3, newval);
    }
}

static void sdhci_iproc_writeb(struct sdhci_host* host, int reg, u8 val)
{
    u32 oldval = sdhci_iproc_readl(host, (reg & ~3));
    u32 byte_shift = REG_OFFSET_IN_BITS(reg);
    u32 mask = 0xff << byte_shift;
    u32 newval = (oldval & ~mask) | (val << byte_shift);

    sdhci_iproc_writel(host, reg & ~3, newval);
}

const struct sdhci_host_ops sdhci_iproc_ops = {
    .read_l = sdhci_iproc_readl,
    .read_w = sdhci_iproc_readw,
    .read_b = sdhci_iproc_readb,
    .write_l = sdhci_iproc_writel,
    .write_w = sdhci_iproc_writew,
    .write_b = sdhci_iproc_writeb,

    .set_clock = sdhci_set_clock,
    .set_bus_width = sdhci_set_bus_width,
};

static int fdt_scan_sdhci_iproc(void* blob, unsigned long offset,
                                const char* name, int depth, void* arg)
{
    struct sdhci_host* host;

    if (!of_flat_dt_match(blob, offset, bcm2835_compat)) return 0;

    host = sdhci_of_init(blob, offset, &sdhci_iproc_ops,
                         sizeof(struct sdhci_iproc_host));
    if (!host) return 0;

    sdhci_add_host(host);
    return 1;
}

void sdhci_iproc_scan(void)
{
    of_scan_fdt(fdt_scan_sdhci_iproc, NULL, boot_params);
}
