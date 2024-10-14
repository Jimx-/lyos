#include <lyos/const.h>
#include <stddef.h>
#include <unistd.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <lyos/irqctl.h>
#include <errno.h>
#include <asm/io.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "dwc2.h"

#define NAME "dwc2-usb"

extern void* boot_params;

static const char* const dwc2_compat[] = {"brcm,bcm2708-usb", NULL};

unsigned int dwc2_op_mode(struct dwc2_hsotg* hsotg)
{
    u32 ghwcfg2 = dwc2_readl(hsotg, GHWCFG2);

    return (ghwcfg2 & GHWCFG2_OP_MODE_MASK) >> GHWCFG2_OP_MODE_SHIFT;
}

int dwc2_hw_is_otg(struct dwc2_hsotg* hsotg)
{
    unsigned int op_mode = dwc2_op_mode(hsotg);

    return (op_mode == GHWCFG2_OP_MODE_HNP_SRP_CAPABLE) ||
           (op_mode == GHWCFG2_OP_MODE_SRP_ONLY_CAPABLE) ||
           (op_mode == GHWCFG2_OP_MODE_NO_HNP_SRP_CAPABLE);
}

int dwc2_hw_is_host(struct dwc2_hsotg* hsotg)
{
    unsigned int op_mode = dwc2_op_mode(hsotg);

    return (op_mode == GHWCFG2_OP_MODE_SRP_CAPABLE_HOST) ||
           (op_mode == GHWCFG2_OP_MODE_NO_SRP_CAPABLE_HOST);
}

int dwc2_hw_is_device(struct dwc2_hsotg* hsotg)
{
    unsigned int op_mode = dwc2_op_mode(hsotg);

    return (op_mode == GHWCFG2_OP_MODE_SRP_CAPABLE_DEVICE) ||
           (op_mode == GHWCFG2_OP_MODE_NO_SRP_CAPABLE_DEVICE);
}

int dwc2_hsotg_wait_bit_set(struct dwc2_hsotg* hsotg, u32 offset, u32 mask,
                            u32 timeout)
{
    u32 i;

    for (i = 0; i < timeout; i++) {
        if (dwc2_readl(hsotg, offset) & mask) return 0;
        usleep(1);
    }

    return ETIMEDOUT;
}

int dwc2_hsotg_wait_bit_clear(struct dwc2_hsotg* hsotg, u32 offset, u32 mask,
                              u32 timeout)
{
    u32 i;

    for (i = 0; i < timeout; i++) {
        if (!(dwc2_readl(hsotg, offset) & mask)) return 0;
        usleep(1);
    }

    return ETIMEDOUT;
}

static int dwc2_check_core_endianness(struct dwc2_hsotg* hsotg)
{
    u32 snpsid;

    snpsid = readl(hsotg->regs + GSNPSID);
    if ((snpsid & GSNPSID_ID_MASK) == DWC2_OTG_ID ||
        (snpsid & GSNPSID_ID_MASK) == DWC2_FS_IOT_ID ||
        (snpsid & GSNPSID_ID_MASK) == DWC2_HS_IOT_ID)
        return FALSE;
    return TRUE;
}

int dwc2_check_core_version(struct dwc2_hsotg* hsotg)
{
    struct dwc2_hw_params* hw = &hsotg->hw_params;

    hw->snpsid = dwc2_readl(hsotg, GSNPSID);
    if ((hw->snpsid & GSNPSID_ID_MASK) != DWC2_OTG_ID &&
        (hw->snpsid & GSNPSID_ID_MASK) != DWC2_FS_IOT_ID &&
        (hw->snpsid & GSNPSID_ID_MASK) != DWC2_HS_IOT_ID) {
        return ENODEV;
    }

    printl(NAME ": Core Release: %1x.%1x%1x%1x (snpsid=%x)\n",
           hw->snpsid >> 12 & 0xf, hw->snpsid >> 8 & 0xf, hw->snpsid >> 4 & 0xf,
           hw->snpsid & 0xf, hw->snpsid);
    return 0;
}

int dwc2_core_reset(struct dwc2_hsotg* hsotg)
{
    u32 greset;

    greset = dwc2_readl(hsotg, GRSTCTL);
    greset |= GRSTCTL_CSFTRST;
    dwc2_writel(hsotg, greset, GRSTCTL);

    if ((hsotg->hw_params.snpsid & DWC2_CORE_REV_MASK) <
        (DWC2_CORE_REV_4_20a & DWC2_CORE_REV_MASK)) {
        if (dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_CSFTRST, 10000)) {
            return EBUSY;
        }
    } else {
        if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL, GRSTCTL_CSFTRST_DONE,
                                    10000)) {
            return EBUSY;
        }
        greset = dwc2_readl(hsotg, GRSTCTL);
        greset &= ~GRSTCTL_CSFTRST;
        greset |= GRSTCTL_CSFTRST_DONE;
        dwc2_writel(hsotg, greset, GRSTCTL);
    }

    if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL, GRSTCTL_AHBIDLE, 10000)) {
        return EBUSY;
    }
    printl("Reset done\n");
    return 0;
}

int dwc2_get_hwparams(struct dwc2_hsotg* hsotg)
{
    struct dwc2_hw_params* hw = &hsotg->hw_params;
    unsigned int width;
    u32 hwcfg1, hwcfg2, hwcfg3, hwcfg4;
    u32 grxfsiz;

    hwcfg1 = dwc2_readl(hsotg, GHWCFG1);
    hwcfg2 = dwc2_readl(hsotg, GHWCFG2);
    hwcfg3 = dwc2_readl(hsotg, GHWCFG3);
    hwcfg4 = dwc2_readl(hsotg, GHWCFG4);
    grxfsiz = dwc2_readl(hsotg, GRXFSIZ);

    /* hwcfg1 */
    hw->dev_ep_dirs = hwcfg1;

    /* hwcfg2 */
    hw->op_mode = (hwcfg2 & GHWCFG2_OP_MODE_MASK) >> GHWCFG2_OP_MODE_SHIFT;
    hw->arch =
        (hwcfg2 & GHWCFG2_ARCHITECTURE_MASK) >> GHWCFG2_ARCHITECTURE_SHIFT;
    hw->enable_dynamic_fifo = !!(hwcfg2 & GHWCFG2_DYNAMIC_FIFO);
    hw->host_channels = 1 + ((hwcfg2 & GHWCFG2_NUM_HOST_CHAN_MASK) >>
                             GHWCFG2_NUM_HOST_CHAN_SHIFT);
    hw->hs_phy_type =
        (hwcfg2 & GHWCFG2_HS_PHY_TYPE_MASK) >> GHWCFG2_HS_PHY_TYPE_SHIFT;
    hw->fs_phy_type =
        (hwcfg2 & GHWCFG2_FS_PHY_TYPE_MASK) >> GHWCFG2_FS_PHY_TYPE_SHIFT;
    hw->num_dev_ep =
        (hwcfg2 & GHWCFG2_NUM_DEV_EP_MASK) >> GHWCFG2_NUM_DEV_EP_SHIFT;
    hw->nperio_tx_q_depth = (hwcfg2 & GHWCFG2_NONPERIO_TX_Q_DEPTH_MASK) >>
                            GHWCFG2_NONPERIO_TX_Q_DEPTH_SHIFT << 1;
    hw->host_perio_tx_q_depth = (hwcfg2 & GHWCFG2_HOST_PERIO_TX_Q_DEPTH_MASK) >>
                                GHWCFG2_HOST_PERIO_TX_Q_DEPTH_SHIFT << 1;
    hw->dev_token_q_depth = (hwcfg2 & GHWCFG2_DEV_TOKEN_Q_DEPTH_MASK) >>
                            GHWCFG2_DEV_TOKEN_Q_DEPTH_SHIFT;

    /* hwcfg3 */
    width = (hwcfg3 & GHWCFG3_XFER_SIZE_CNTR_WIDTH_MASK) >>
            GHWCFG3_XFER_SIZE_CNTR_WIDTH_SHIFT;
    hw->max_transfer_size = (1 << (width + 11)) - 1;
    width = (hwcfg3 & GHWCFG3_PACKET_SIZE_CNTR_WIDTH_MASK) >>
            GHWCFG3_PACKET_SIZE_CNTR_WIDTH_SHIFT;
    hw->max_packet_count = (1 << (width + 4)) - 1;
    hw->i2c_enable = !!(hwcfg3 & GHWCFG3_I2C);
    hw->total_fifo_size =
        (hwcfg3 & GHWCFG3_DFIFO_DEPTH_MASK) >> GHWCFG3_DFIFO_DEPTH_SHIFT;
    hw->lpm_mode = !!(hwcfg3 & GHWCFG3_OTG_LPM_EN);

    /* hwcfg4 */
    hw->en_multiple_tx_fifo = !!(hwcfg4 & GHWCFG4_DED_FIFO_EN);
    hw->num_dev_perio_in_ep = (hwcfg4 & GHWCFG4_NUM_DEV_PERIO_IN_EP_MASK) >>
                              GHWCFG4_NUM_DEV_PERIO_IN_EP_SHIFT;
    hw->num_dev_in_eps =
        (hwcfg4 & GHWCFG4_NUM_IN_EPS_MASK) >> GHWCFG4_NUM_IN_EPS_SHIFT;
    hw->dma_desc_enable = !!(hwcfg4 & GHWCFG4_DESC_DMA);
    hw->power_optimized = !!(hwcfg4 & GHWCFG4_POWER_OPTIMIZ);
    hw->hibernation = !!(hwcfg4 & GHWCFG4_HIBER);
    hw->utmi_phy_data_width = (hwcfg4 & GHWCFG4_UTMI_PHY_DATA_WIDTH_MASK) >>
                              GHWCFG4_UTMI_PHY_DATA_WIDTH_SHIFT;
    hw->acg_enable = !!(hwcfg4 & GHWCFG4_ACG_SUPPORTED);
    hw->ipg_isoc_en = !!(hwcfg4 & GHWCFG4_IPG_ISOC_SUPPORTED);
    hw->service_interval_mode = !!(hwcfg4 & GHWCFG4_SERVICE_INTERVAL_SUPPORTED);

    /* fifo sizes */
    hw->rx_fifo_size = (grxfsiz & GRXFSIZ_DEPTH_MASK) >> GRXFSIZ_DEPTH_SHIFT;

    /* dwc2_get_host_hwparams(hsotg); */
    /* dwc2_get_dev_hwparams(hsotg); */

    return 0;
}

static void dwc2_set_bcm_params(struct dwc2_hsotg* hsotg)
{
    struct dwc2_core_params* p = &hsotg->params;

    p->host_rx_fifo_size = 774;
    p->max_transfer_size = 65535;
    p->max_packet_count = 511;
    p->ahbcfg = 0x10;
}

static int dwc2_init_params(struct dwc2_hsotg* hsotg)
{
    dwc2_set_bcm_params(hsotg);
    return 0;
}

static int fdt_scan_dwc2(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct dwc2_hsotg* hsotg;
    phys_bytes base, size;
    int ret;

    hsotg = malloc(sizeof(struct dwc2_hsotg));
    if (!hsotg) return 0;

    memset(hsotg, 0, sizeof(*hsotg));

    if (!of_flat_dt_match(blob, offset, dwc2_compat)) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) goto error_free;

    hsotg->regs = mm_map_phys(SELF, base, size, MMP_IO);
    if (hsotg->regs == MAP_FAILED) goto error_free;

    hsotg->irq = irq_of_parse_and_map(blob, offset, 0);
    if (!hsotg->irq) goto error_free;

    hsotg->needs_byte_swap = dwc2_check_core_endianness(hsotg);

    ret = dwc2_check_core_version(hsotg);
    if (ret) goto error_free;

    ret = dwc2_core_reset(hsotg);
    if (ret) goto error_free;

    ret = dwc2_init_params(hsotg);
    if (ret) goto error_free;

    return 1;
error_free:
    free(hsotg);
    return 0;
}

void dwc2_scan(void) { of_scan_fdt(fdt_scan_dwc2, NULL, boot_params); }
