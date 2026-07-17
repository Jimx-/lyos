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
#include "hcd.h"
#include "usb.h"

#define NAME "dwc2-usb"

extern void* boot_params;

static const char* const dwc2_compat[] = {
    "brcm,bcm2708-usb", "brcm,bcm2835-usb", "snps,dwc2", NULL};

static inline struct dwc2_hsotg* hcd_to_dwc2(struct usb_hcd* hcd)
{
    return (struct dwc2_hsotg*)hcd->hcd_priv[0];
}

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

static u32 dwc2_hprt0_read(struct dwc2_hsotg* hsotg)
{
    u32 hprt0 = dwc2_readl(hsotg, HPRT0);

    /* Change bits are W1C, and writing one to PRTENA disables the port. */
    return hprt0 &
           ~(HPRT0_ENA | HPRT0_ENACHG | HPRT0_CONNDET | HPRT0_OVRCURRCHG);
}

static void dwc2_latch_port_changes(struct dwc2_hsotg* hsotg)
{
    u32 hprt0 = dwc2_readl(hsotg, HPRT0);
    u32 ack = hprt0 & (HPRT0_CONNDET | HPRT0_ENACHG | HPRT0_OVRCURRCHG);

    if (hprt0 & HPRT0_CONNDET) hsotg->port_change |= USB_PORT_STAT_C_CONNECTION;
    if (hprt0 & HPRT0_ENACHG) hsotg->port_change |= USB_PORT_STAT_C_ENABLE;
    if (hprt0 & HPRT0_OVRCURRCHG)
        hsotg->port_change |= USB_PORT_STAT_C_OVERCURRENT;

    if (ack) dwc2_writel(hsotg, dwc2_hprt0_read(hsotg) | ack, HPRT0);
}

static void dwc2_flush_fifos(struct dwc2_hsotg* hsotg)
{
    dwc2_writel(hsotg, GRSTCTL_RXFFLSH, GRSTCTL);
    dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_RXFFLSH, 10000);
    dwc2_writel(hsotg, GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM(0x10), GRSTCTL);
    dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_TXFFLSH, 10000);
}

static void dwc2_halt_channels(struct dwc2_hsotg* hsotg)
{
    unsigned int channel;

    for (channel = 0; channel < hsotg->hw_params.host_channels; channel++) {
        u32 hcchar = dwc2_readl(hsotg, HCCHAR(channel));

        hcchar &= ~HCCHAR_CHENA;
        hcchar |= HCCHAR_CHDIS;
        dwc2_writel(hsotg, hcchar, HCCHAR(channel));
        dwc2_writel(hsotg, ~0U, HCINT(channel));
        dwc2_writel(hsotg, 0, HCINTMSK(channel));
    }
}

static int dwc2_host_init(struct dwc2_hsotg* hsotg)
{
    u32 val;

    if (dwc2_hw_is_device(hsotg)) return ENODEV;

    val = dwc2_readl(hsotg, GUSBCFG);
    val &= ~GUSBCFG_FORCEDEVMODE;
    val |= GUSBCFG_FORCEHOSTMODE;
    dwc2_writel(hsotg, val, GUSBCFG);
    usleep(25000);

    if (!(dwc2_readl(hsotg, GINTSTS) & GINTSTS_CURMODE_HOST)) return ENODEV;

    dwc2_flush_fifos(hsotg);
    dwc2_halt_channels(hsotg);

    val = dwc2_readl(hsotg, HCFG);
    val &= ~HCFG_FSLSPCLKSEL_MASK;
    val |= HCFG_FSLSPCLKSEL_30_60_MHZ;
    dwc2_writel(hsotg, val, HCFG);

    dwc2_writel(hsotg, ~0U, GINTSTS);
    dwc2_writel(hsotg, GINTSTS_PRTINT | GINTSTS_DISCONNINT | GINTSTS_HCHINT,
                GINTMSK);
    val = hsotg->params.ahbcfg | GAHBCFG_DMA_EN | GAHBCFG_GLBL_INTR_EN;
    dwc2_writel(hsotg, val, GAHBCFG);

    val = dwc2_hprt0_read(hsotg) | HPRT0_PWR;
    dwc2_writel(hsotg, val, HPRT0);
    return 0;
}

static int dwc2_setup(struct usb_hcd* hcd)
{
    return dwc2_host_init(hcd_to_dwc2(hcd));
}

static int dwc2_start(struct usb_hcd* hcd)
{
    (void)hcd;
    return 0;
}

static int dwc2_channel_xfer(struct dwc2_hsotg* hsotg, struct urb* urb,
                             phys_bytes dma, unsigned int length, int dir_in,
                             unsigned int pid)
{
    const unsigned int channel = 0;
    unsigned int epnum = usb_pipeendpoint(urb->pipe);
    unsigned int maxp = usb_maxpacket(urb->dev, urb->pipe);
    unsigned int packets, eptype, retries = 0;
    u32 hcchar, hcint, hctsiz;

    if (!maxp) return EINVAL;
    packets = length ? (length + maxp - 1) / maxp : 1;
    if (packets > hsotg->params.max_packet_count ||
        length > hsotg->params.max_transfer_size)
        return E2BIG;

    switch (usb_pipetype(urb->pipe)) {
    case PIPE_CONTROL:
        eptype = 0;
        break;
    case PIPE_ISOCHRONOUS:
        eptype = 1;
        break;
    case PIPE_BULK:
        eptype = 2;
        break;
    case PIPE_INTERRUPT:
        eptype = 3;
        break;
    default:
        return EINVAL;
    }

retry:
    dwc2_writel(hsotg, ~0U, HCINT(channel));
    hctsiz = (length & TSIZ_XFERSIZE_MASK) | (packets << TSIZ_PKTCNT_SHIFT) |
             (pid << TSIZ_SC_MC_PID_SHIFT);
    dwc2_writel(hsotg, hctsiz, HCTSIZ(channel));
    dwc2_writel(hsotg, (u32)dma, HCDMA(channel));
    dwc2_writel(hsotg, 0, HCSPLT(channel));

    hcchar = maxp | (epnum << HCCHAR_EPNUM_SHIFT) |
             (eptype << HCCHAR_EPTYPE_SHIFT) |
             (urb->dev->devnum << HCCHAR_DEVADDR_SHIFT);
    if (dir_in) hcchar |= HCCHAR_EPDIR;
    if (urb->dev->speed == USB_SPEED_LOW) hcchar |= HCCHAR_LSPDDEV;
    if (dwc2_readl(hsotg, HFNUM) & 1) hcchar |= HCCHAR_ODDFRM;
    dwc2_writel(hsotg, hcchar | HCCHAR_CHENA, HCCHAR(channel));

    for (unsigned int timeout = 0; timeout < 500000; timeout++) {
        hcint = dwc2_readl(hsotg, HCINT(channel));
        if (hcint & (HCINTMSK_CHHLTD | HCINTMSK_XFERCOMPL | HCINTMSK_STALL |
                     HCINTMSK_XACTERR | HCINTMSK_BBLERR | HCINTMSK_AHBERR |
                     HCINTMSK_DATATGLERR | HCINTMSK_NAK))
            break;
        usleep(1);
    }
    hcint = dwc2_readl(hsotg, HCINT(channel));
    dwc2_writel(hsotg, hcint, HCINT(channel));

    if (hcint & HCINTMSK_XFERCOMPL) {
        hctsiz = dwc2_readl(hsotg, HCTSIZ(channel));
        return length - (hctsiz & TSIZ_XFERSIZE_MASK);
    }
    if ((hcint & (HCINTMSK_NAK | HCINTMSK_NYET)) && usb_pipeint(urb->pipe))
        return -EAGAIN;
    if ((hcint & (HCINTMSK_NAK | HCINTMSK_NYET)) && retries++ < 1000) {
        usleep(100);
        goto retry;
    }
    if (hcint & HCINTMSK_STALL) return -EPIPE;
    if (hcint & HCINTMSK_BBLERR) return -EOVERFLOW;
    if (hcint & HCINTMSK_DATATGLERR) return -EILSEQ;
    if (hcint & (HCINTMSK_XACTERR | HCINTMSK_AHBERR)) return -EIO;
    return -ETIMEDOUT;
}

static void dwc2_start_intr_channel(struct dwc2_hsotg* hsotg,
                                    unsigned int channel)
{
    struct urb* urb = hsotg->pending_intr_urbs[channel];
    unsigned int epnum = usb_pipeendpoint(urb->pipe);
    unsigned int maxp = usb_maxpacket(urb->dev, urb->pipe);
    unsigned int length = urb->transfer_buffer_length;
    unsigned int packets = length ? (length + maxp - 1) / maxp : 1;
    unsigned int pid = usb_gettoggle(urb->dev, epnum, !usb_pipein(urb->pipe))
                           ? TSIZ_SC_MC_PID_DATA1
                           : TSIZ_SC_MC_PID_DATA0;
    u32 hcchar, hctsiz;

    dwc2_writel(hsotg, ~0U, HCINT(channel));
    hctsiz = (length & TSIZ_XFERSIZE_MASK) | (packets << TSIZ_PKTCNT_SHIFT) |
             (pid << TSIZ_SC_MC_PID_SHIFT);
    dwc2_writel(hsotg, hctsiz, HCTSIZ(channel));
    dwc2_writel(hsotg, (u32)urb->transfer_phys, HCDMA(channel));
    dwc2_writel(hsotg, 0, HCSPLT(channel));
    dwc2_writel(hsotg,
                HCINTMSK_XFERCOMPL | HCINTMSK_CHHLTD | HCINTMSK_NAK |
                    HCINTMSK_NYET | HCINTMSK_STALL | HCINTMSK_XACTERR |
                    HCINTMSK_BBLERR | HCINTMSK_AHBERR | HCINTMSK_DATATGLERR,
                HCINTMSK(channel));
    dwc2_writel(hsotg, dwc2_readl(hsotg, HAINTMSK) | BIT(channel), HAINTMSK);

    hcchar = maxp | (epnum << HCCHAR_EPNUM_SHIFT) | (3 << HCCHAR_EPTYPE_SHIFT) |
             (urb->dev->devnum << HCCHAR_DEVADDR_SHIFT);
    if (usb_pipein(urb->pipe)) hcchar |= HCCHAR_EPDIR;
    if (urb->dev->speed == USB_SPEED_LOW) hcchar |= HCCHAR_LSPDDEV;
    if (dwc2_readl(hsotg, HFNUM) & 1) hcchar |= HCCHAR_ODDFRM;
    dwc2_writel(hsotg, hcchar | HCCHAR_CHENA, HCCHAR(channel));
}

static void dwc2_intr_schedule(struct timer_list* timer)
{
    struct dwc2_hsotg* hsotg = timer->arg;
    unsigned int channel, schedule_map = hsotg->intr_schedule_map;

    hsotg->intr_schedule_map = 0;
    for (channel = 1; channel < hsotg->hw_params.host_channels; channel++) {
        if ((schedule_map & BIT(channel)) && hsotg->pending_intr_urbs[channel])
            dwc2_start_intr_channel(hsotg, channel);
    }
}

static int dwc2_urb_enqueue(struct usb_hcd* hcd, struct urb* urb)
{
    struct dwc2_hsotg* hsotg = hcd_to_dwc2(hcd);
    unsigned int epnum = usb_pipeendpoint(urb->pipe), i;
    int dir_in = usb_pipein(urb->pipe);
    int pid, ret, status = 0;

    if (usb_pipeisoc(urb->pipe)) return ENOTSUP;

    ret = usb_hcd_link_urb_to_ep(hcd, urb);
    if (ret) return ret;
    urb->hc_priv = hsotg;
    urb->actual_length = 0;

    /* A completion callback may resubmit an interrupt URB immediately. */
    if (usb_pipeint(urb->pipe)) {
        for (i = 1; i < hsotg->hw_params.host_channels; i++) {
            if (!hsotg->pending_intr_urbs[i]) break;
        }
        if (i == hsotg->hw_params.host_channels) {
            usb_hcd_unlink_urb_from_ep(hcd, urb);
            return EBUSY;
        }
        hsotg->pending_intr_urbs[i] = urb;
        hsotg->intr_schedule_map |= BIT(i);
        if (hsotg->intr_schedule_timer.expire_time == TIMER_UNSET)
            set_timer(&hsotg->intr_schedule_timer, 1, dwc2_intr_schedule,
                      hsotg);
        return 0;
    }

    if (usb_pipecontrol(urb->pipe)) {
        ret = dwc2_channel_xfer(hsotg, urb, urb->setup_phys,
                                sizeof(struct usb_ctrlrequest), FALSE,
                                TSIZ_SC_MC_PID_SETUP);
        if (ret < 0) goto done;
        if (urb->transfer_buffer_length) {
            ret = dwc2_channel_xfer(hsotg, urb, urb->transfer_phys,
                                    urb->transfer_buffer_length, dir_in,
                                    TSIZ_SC_MC_PID_DATA1);
            if (ret < 0) goto done;
            urb->actual_length = ret;
        }
        ret =
            dwc2_channel_xfer(hsotg, urb, 0, 0, !dir_in, TSIZ_SC_MC_PID_DATA1);
    } else {
        pid = usb_gettoggle(urb->dev, epnum, !dir_in) ? TSIZ_SC_MC_PID_DATA1
                                                      : TSIZ_SC_MC_PID_DATA0;
        ret = dwc2_channel_xfer(hsotg, urb, urb->transfer_phys,
                                urb->transfer_buffer_length, dir_in, pid);
        if (ret >= 0) {
            urb->actual_length = ret;
            if (((ret + usb_maxpacket(urb->dev, urb->pipe) - 1) /
                 usb_maxpacket(urb->dev, urb->pipe)) &
                1)
                usb_dotoggle(urb->dev, epnum, !dir_in);
        }
    }

done:
    if (ret < 0) status = -ret;
    usb_hcd_unlink_urb_from_ep(hcd, urb);
    usb_hcd_giveback_urb(hcd, urb, status);
    return 0;
}

static void dwc2_complete_intr_channel(struct dwc2_hsotg* hsotg,
                                       unsigned int channel)
{
    struct usb_hcd* hcd = hsotg->hcd;
    struct urb* urb = hsotg->pending_intr_urbs[channel];
    unsigned int epnum, maxp;
    int status = 0;
    u32 hcint, hctsiz;

    if (!urb) return;

    hcint = dwc2_readl(hsotg, HCINT(channel));
    dwc2_writel(hsotg, hcint, HCINT(channel));

    if (hcint & (HCINTMSK_NAK | HCINTMSK_NYET)) {
        dwc2_writel(hsotg, 0, HCINTMSK(channel));
        hsotg->intr_schedule_map |= BIT(channel);
        if (hsotg->intr_schedule_timer.expire_time == TIMER_UNSET)
            set_timer(&hsotg->intr_schedule_timer, 1, dwc2_intr_schedule,
                      hsotg);
        return;
    }

    hsotg->pending_intr_urbs[channel] = NULL;
    dwc2_writel(hsotg, 0, HCINTMSK(channel));
    dwc2_writel(hsotg, dwc2_readl(hsotg, HAINTMSK) & ~BIT(channel), HAINTMSK);

    if (hcint & HCINTMSK_XFERCOMPL) {
        hctsiz = dwc2_readl(hsotg, HCTSIZ(channel));
        urb->actual_length =
            urb->transfer_buffer_length - (hctsiz & TSIZ_XFERSIZE_MASK);
        epnum = usb_pipeendpoint(urb->pipe);
        maxp = usb_maxpacket(urb->dev, urb->pipe);
        if (((urb->actual_length + maxp - 1) / maxp) & 1)
            usb_dotoggle(urb->dev, epnum, !usb_pipein(urb->pipe));
    } else if (hcint & HCINTMSK_STALL)
        status = EPIPE;
    else if (hcint & HCINTMSK_BBLERR)
        status = EOVERFLOW;
    else if (hcint & HCINTMSK_DATATGLERR)
        status = EILSEQ;
    else
        status = EIO;

    usb_hcd_unlink_urb_from_ep(hcd, urb);
    usb_hcd_giveback_urb(hcd, urb, status);
}

static void dwc2_irq(struct usb_hcd* hcd)
{
    struct dwc2_hsotg* hsotg = hcd_to_dwc2(hcd);
    unsigned int channel;
    u32 status = dwc2_readl(hsotg, GINTSTS) & dwc2_readl(hsotg, GINTMSK);

    if (status & GINTSTS_HCHINT) {
        u32 haint = dwc2_readl(hsotg, HAINT) & dwc2_readl(hsotg, HAINTMSK);

        for (channel = 1; channel < hsotg->hw_params.host_channels; channel++) {
            if (haint & BIT(channel))
                dwc2_complete_intr_channel(hsotg, channel);
        }
    }
    if (status & GINTSTS_PRTINT) dwc2_latch_port_changes(hsotg);
    if (status & (GINTSTS_PRTINT | GINTSTS_DISCONNINT))
        usb_hcd_poll_rh_status(hcd);
    dwc2_writel(hsotg, status & ~GINTSTS_PRTINT, GINTSTS);
    irq_enable(&hcd->irq_hook);
}

static int dwc2_hub_status_data(struct usb_hcd* hcd, char* buf)
{
    struct dwc2_hsotg* hsotg = hcd_to_dwc2(hcd);

    dwc2_latch_port_changes(hsotg);

    buf[0] = 0;
    if (hsotg->port_change) buf[0] = BIT(1);
    return buf[0] ? 1 : 0;
}

static int dwc2_hub_control(struct usb_hcd* hcd, u16 typeReq, u16 wValue,
                            u16 wIndex, char* buf, u16 wLength)
{
    struct dwc2_hsotg* hsotg = hcd_to_dwc2(hcd);
    u32 hprt0, val;
    u16 status = 0, change = hsotg->port_change;

    (void)wLength;
    switch (typeReq) {
    case GetHubDescriptor: {
        struct usb_hub_descriptor* desc = (void*)buf;
        memset(desc, 0, sizeof(*desc));
        desc->bDescLength = 9;
        desc->bDescriptorType = USB_DT_HUB;
        desc->bNbrPorts = 1;
        desc->wHubCharacteristics = cpu_to_le16(0x0009);
        desc->bPwrOn2PwrGood = 1;
        desc->u.hs.DeviceRemovable[0] = 0;
        desc->u.hs.DeviceRemovable[1] = 0xff;
        return 9;
    }
    case GetHubStatus:
        memset(buf, 0, 4);
        return 0;
    case GetPortStatus:
        if (wIndex != 1) goto error;
        dwc2_latch_port_changes(hsotg);
        hprt0 = dwc2_readl(hsotg, HPRT0);
        if (hprt0 & HPRT0_CONNSTS) status |= USB_PORT_STAT_CONNECTION;
        if (hprt0 & HPRT0_ENA) status |= USB_PORT_STAT_ENABLE;
        if (hprt0 & HPRT0_SUSP) status |= USB_PORT_STAT_SUSPEND;
        if (hprt0 & HPRT0_OVRCURRACT) status |= USB_PORT_STAT_OVERCURRENT;
        if (hprt0 & HPRT0_RST) status |= USB_PORT_STAT_RESET;
        if (hprt0 & HPRT0_PWR) status |= USB_PORT_STAT_POWER;
        if ((hprt0 & HPRT0_SPD_MASK) ==
            (HPRT0_SPD_LOW_SPEED << HPRT0_SPD_SHIFT))
            status |= USB_PORT_STAT_LOW_SPEED;
        else if ((hprt0 & HPRT0_SPD_MASK) ==
                 (HPRT0_SPD_HIGH_SPEED << HPRT0_SPD_SHIFT))
            status |= USB_PORT_STAT_HIGH_SPEED;
        change = hsotg->port_change;
        ((u16*)buf)[0] = cpu_to_le16(status);
        ((u16*)buf)[1] = cpu_to_le16(change);
        return 0;
    case SetPortFeature:
        if (wIndex != 1) goto error;
        val = dwc2_hprt0_read(hsotg);
        if (wValue == USB_PORT_FEAT_POWER)
            val |= HPRT0_PWR;
        else if (wValue == USB_PORT_FEAT_RESET) {
            val |= HPRT0_RST;
            dwc2_writel(hsotg, val, HPRT0);
            usleep(50000);
            val = dwc2_hprt0_read(hsotg) & ~HPRT0_RST;
            hsotg->port_change |= USB_PORT_STAT_C_RESET;
        } else if (wValue == USB_PORT_FEAT_SUSPEND)
            val |= HPRT0_SUSP;
        else
            goto error;
        dwc2_writel(hsotg, val, HPRT0);
        return 0;
    case ClearPortFeature:
        if (wIndex != 1) goto error;
        val = dwc2_hprt0_read(hsotg);
        switch (wValue) {
        case USB_PORT_FEAT_ENABLE:
            val &= ~HPRT0_ENA;
            break;
        case USB_PORT_FEAT_POWER:
            val &= ~HPRT0_PWR;
            break;
        case USB_PORT_FEAT_SUSPEND:
            val &= ~HPRT0_SUSP;
            val |= HPRT0_RES;
            dwc2_writel(hsotg, val, HPRT0);
            usleep(20000);
            val &= ~HPRT0_RES;
            hsotg->port_change |= USB_PORT_STAT_C_SUSPEND;
            break;
        case USB_PORT_FEAT_C_CONNECTION:
            hsotg->port_change &= ~USB_PORT_STAT_C_CONNECTION;
            break;
        case USB_PORT_FEAT_C_ENABLE:
            hsotg->port_change &= ~USB_PORT_STAT_C_ENABLE;
            break;
        case USB_PORT_FEAT_C_OVER_CURRENT:
            hsotg->port_change &= ~USB_PORT_STAT_C_OVERCURRENT;
            break;
        case USB_PORT_FEAT_C_RESET:
            hsotg->port_change &= ~USB_PORT_STAT_C_RESET;
            break;
        case USB_PORT_FEAT_C_SUSPEND:
            hsotg->port_change &= ~USB_PORT_STAT_C_SUSPEND;
            break;
        default:
            goto error;
        }
        dwc2_writel(hsotg, val, HPRT0);
        return 0;
    default:
        goto error;
    }
error:
    return -EPIPE;
}

static const struct hc_driver dwc2_hc_driver = {
    .description = NAME,
    .product_desc = "DWC2 Host Controller",
    .hcd_priv_size = sizeof(unsigned long),
    .flags = HCD_USB2 | HCD_MEMORY,
    .irq = dwc2_irq,
    .setup = dwc2_setup,
    .start = dwc2_start,
    .urb_enqueue = dwc2_urb_enqueue,
    .hub_status_data = dwc2_hub_status_data,
    .hub_control = dwc2_hub_control,
};

static int fdt_scan_dwc2(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct dwc2_hsotg* hsotg;
    phys_bytes base, size;
    int ret;

    hsotg = malloc(sizeof(struct dwc2_hsotg));
    if (!hsotg) return 0;

    memset(hsotg, 0, sizeof(*hsotg));
    init_timer(&hsotg->intr_schedule_timer);

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

    ret = dwc2_get_hwparams(hsotg);
    if (ret) goto error_free;

    hsotg->params.host_channels = hsotg->hw_params.host_channels;
    hsotg->params.max_transfer_size = min(hsotg->params.max_transfer_size,
                                          hsotg->hw_params.max_transfer_size);
    hsotg->params.max_packet_count =
        min(hsotg->params.max_packet_count, hsotg->hw_params.max_packet_count);

    hsotg->hcd = usb_create_hcd(&dwc2_hc_driver);
    if (!hsotg->hcd) goto error_free;
    hsotg->hcd->hcd_priv[0] = (unsigned long)hsotg;
    hsotg->hcd->regs = hsotg->regs;

    ret = usb_hcd_add(hsotg->hcd, hsotg->irq);
    if (ret) {
        usb_put_hcd(hsotg->hcd);
        goto error_free;
    }

    printl(NAME ": registered host controller, irq %d, %u channels\n",
           hsotg->irq, hsotg->hw_params.host_channels);

    return 1;
error_free:
    free(hsotg);
    return 0;
}

void dwc2_scan(void) { of_scan_fdt(fdt_scan_dwc2, NULL, boot_params); }
