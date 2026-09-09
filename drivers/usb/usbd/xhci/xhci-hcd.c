#define __LINUX_ERRNO_EXTENSIONS__
#include <lyos/sysutils.h>
#include <asm/io.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/irqctl.h>
#include <lyos/usb.h>
#include <asm/barrier.h>

#include "hcd.h"
#include "xhci.h"

static const char hcd_name[] = "xhci_hcd";

/* Map xHCI completion code to errno */
static const int comp_to_error[] = {
    [COMP_SUCCESS] = 0,
    [COMP_PING_RESPONSE] = EAGAIN,
    [COMP_BABBLE_DETECTED] = EOVERFLOW,
    [COMP_TRANSACTION_ERROR] = EPROTO,
    [COMP_TRB_ERROR] = EINVAL,
    [COMP_STALL_ERROR] = EPIPE,
    [COMP_RESOURCE_ERROR] = ENOMEM,
    [COMP_BANDWIDTH_ERROR] = ENOSPC,
    [COMP_NO_SLOTS_ERROR] = ENOSPC,
    [COMP_INVALID_STREAM_TYPE_ERROR] = EINVAL,
    [COMP_SLOT_NOT_ENABLED_ERROR] = EINVAL,
    [COMP_ENDPOINT_NOT_ENABLED_ERROR] = EINVAL,
    [COMP_SHORT_PACKET] = 0,
    [COMP_RING_UNDERRUN] = ECOMM,
    [COMP_RING_OVERRUN] = ECOMM,
    [COMP_VF_EVENT_RING_FULL_ERROR] = ENOSPC,
    [COMP_PARAMETER_ERROR] = EINVAL,
    [COMP_BANDWIDTH_OVERRUN_ERROR] = ENOSPC,
    [COMP_CONTEXT_STATE_ERROR] = EINVAL,
    [COMP_NO_PING_RESPONSE_ERROR] = ETIME,
    [COMP_EVENT_RING_FULL_ERROR] = ENOSPC,
    [COMP_INCOMPATIBLE_DEVICE_ERROR] = ENODEV,
    [COMP_MISSED_SERVICE_ERROR] = ECOMM,
    [COMP_COMMAND_RING_STOPPED] = EIO,
    [COMP_COMMAND_ABORTED] = EINTR,
    [COMP_STOPPED] = ECONNRESET,
    [COMP_STOPPED_LENGTH_INVALID] = ECONNRESET,
    [COMP_EXIT_LATENCY_TOO_LARGE_ERROR] = EINVAL,
    [COMP_ISOCH_BUFFER_OVERRUN] = ECOMM,
    [COMP_EVENT_LOST_ERROR] = ESHUTDOWN,
    [COMP_UNDEFINED_ERROR] = EIO,
    [COMP_INVALID_STREAM_ID_ERROR] = EINVAL,
    [COMP_SECONDARY_BANDWIDTH_ERROR] = ENOSPC,
    [COMP_SPLIT_TRANSACTION_ERROR] = EPROTO,
};

int xhci_comp_to_errno(u32 comp_code)
{
    if (comp_code < sizeof(comp_to_error) / sizeof(comp_to_error[0]))
        return comp_to_error[comp_code];
    return EIO;
}

/* =========================================================================
 * HC reset and initialization
 * ========================================================================= */
static int xhci_hc_reset(struct xhci_hcd* xhci)
{
    u32 val;
    int timeout = 1000; /* 1 second max */

    /* Issue reset */
    xhci_writel(xhci, &xhci->op_regs->usbcmd, CMD_RESET);

    /* Wait for reset to complete (controller clears CMD_RESET) */
    do {
        val = xhci_readl(xhci, &xhci->op_regs->usbcmd);
        if (!(val & CMD_RESET)) return 0;
        usleep(1000);
    } while (--timeout > 0);

    return ETIMEDOUT;
}

static int xhci_wait_for_ready(struct xhci_hcd* xhci)
{
    u32 val;
    int timeout = 1000;

    /* Wait for controller not-ready bit to clear */
    do {
        val = xhci_readl(xhci, &xhci->op_regs->usbsts);
        if (!(val & STS_CNR)) return 0;
        usleep(1000);
    } while (--timeout > 0);

    return ETIMEDOUT;
}

/* Discover port information from extended capabilities */
static void xhci_probe_ports(struct xhci_hcd* xhci)
{
    u32 i;
    u32 num_ports = xhci->num_ports;

    xhci->ports = calloc(num_ports, sizeof(struct xhci_port));
    if (!xhci->ports) return;

    for (i = 0; i < num_ports; i++) {
        xhci->ports[i].hw_portnum = i;
        /* TODO: parse extended capabilities to determine per-port
         * USB revision (USB 2.0 vs USB 3.x). For now, infer from
         * the port speed field after connection. */
        xhci->ports[i].majrev = 3;
        xhci->ports[i].minrev = 0;
    }
}

/* =========================================================================
 * hc_driver: setup
 * ========================================================================= */
static int xhci_setup(struct usb_hcd* hcd)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    struct xhci_cap_regs* cap;
    u32 val;
    int retval;

    cap = (struct xhci_cap_regs*)hcd->regs;
    xhci->cap_regs = cap;

    /* Parse capability registers */
    val = xhci_readl(xhci, &cap->hcsparams1);
    xhci->num_slots = HCS_MAX_SLOTS(val);
    xhci->num_intrs = HCS_MAX_INTRS(val);
    xhci->num_ports = HCS_MAX_PORTS(val);

    val = xhci_readl(xhci, &cap->hcsparams2);
    xhci->max_scratch_bufs = HCS_MAX_SCRATCH_BUFS(val);

    val = xhci_readl(xhci, &cap->hccparams1);
    xhci->hcc_params = val;
    xhci->context_size =
        (HCC_64BYTE_CONTEXT(val)) ? XHCI_CTX_SIZE_64 : XHCI_CTX_SIZE_32;

    printl("xhci: xHCI controller found (version %d.%02d), %d ports, "
           "%d slots, %d interrupters\n",
           le16_to_cpu(cap->hciversion) >> 8,
           le16_to_cpu(cap->hciversion) & 0xff, xhci->num_ports,
           xhci->num_slots, xhci->num_intrs);
    printl("xhci: %d-byte contexts, %d scratchpad buffers\n",
           xhci->context_size, xhci->max_scratch_bufs);

    /* Calculate register base pointers */
    {
        u32 hc_length = cap->hc_length;
        u32 dboff = xhci_readl(xhci, &cap->dboff);
        u32 rtsoff = xhci_readl(xhci, &cap->rtsoff);

        xhci->op_regs = (struct xhci_op_regs*)((char*)hcd->regs + hc_length);
        xhci->run_regs = (struct xhci_run_regs*)((char*)hcd->regs + rtsoff);
        xhci->db_base = (char*)hcd->regs + dboff;

        printl("xhci: caps=%p op=%p runtime=%p doorbell=%p\n", cap,
               xhci->op_regs, xhci->run_regs, xhci->db_base);
    }

    /* Probe port layout */
    xhci_probe_ports(xhci);

    /* Reset the controller */
    retval = xhci_hc_reset(xhci);
    if (retval) {
        printl("xhci: controller reset failed (%d)\n", retval);
        return retval;
    }

    /* Allocate all xHCI data structures (DCBAA, rings, etc.) */
    retval = xhci_mem_init(xhci);
    if (retval) {
        printl("xhci: memory init failed (%d)\n", retval);
        return retval;
    }

    /* Determine page size */
    /* TODO: read PAGESIZE register (op offset 0x08 is actually a reserved/
     * page-size register in some implementations). The page size register
     * is at operational register offset 0x08 in xHCI 1.0+.
     * For now, assume 4K. */
    xhci->page_size = 4096;

    return 0;
}

/* =========================================================================
 * hc_driver: start
 * ========================================================================= */
static int xhci_start(struct usb_hcd* hcd)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    u32 val;
    int retval;

    retval = xhci_wait_for_ready(xhci);
    if (retval) return retval;

    /* Set max device slots */
    val = xhci_readl(xhci, &xhci->op_regs->config);
    val &= ~0xff;
    val |= CONFIG_MAX_SLOTS(xhci->num_slots);
    xhci_writel(xhci, &xhci->op_regs->config, val);

    /* Program DCBAAP */
    xhci_writeq(xhci, (u64*)&xhci->op_regs->dcbaap, xhci->dcbaa_dma);

    /* Program command ring control (CRCR) */
    {
        u64 val64 = xhci->cmd_ring->first_seg->dma;
        val64 |= CRCR_RCS; /* set initial cycle state */
        xhci_writeq(xhci, (u64*)&xhci->op_regs->crcr, val64);
    }

    /* Program event ring segment table (ERST) for interrupter 0 */
    {
        volatile struct xhci_intr_regs* ir = &xhci->run_regs->irs[0];

        xhci_writel(xhci, &ir->erstsz, xhci->erst.num_entries);

        xhci_writeq(xhci, (u64*)&ir->erstba, xhci->erst.dma);

        /* Set ERDP to the beginning of the event ring */
        xhci_writeq(xhci, (u64*)&ir->erdp, xhci->event_ring->first_seg->dma);

        /* Enable interrupter */
        val = xhci_readl(xhci, &ir->iman);
        val |= IMAN_IE;
        xhci_writel(xhci, &ir->iman, val);
    }

    /* Clear any pending interrupt status */
    xhci_writel(xhci, &xhci->op_regs->usbsts, STS_EINT | STS_PCD);

    /* Enable interrupts and start the controller */
    val = xhci_readl(xhci, &xhci->op_regs->usbcmd);
    val |= CMD_EIE | CMD_HSEE | CMD_RUN;
    xhci_writel(xhci, &xhci->op_regs->usbcmd, val);
    xhci->usbcmd = val;

    /* Wait for controller to be ready */
    retval = xhci_wait_for_ready(xhci);
    if (retval) {
        printl("xhci: controller not ready after start (%d)\n", retval);
        return retval;
    }

    /* Power on all ports (if port power control is supported) */
    if (xhci->hcc_params & HCC_PPC(xhci->hcc_params)) {
        unsigned int i;
        for (i = 0; i < xhci->num_ports; i++) {
            u32* portsc = (u32*)xhci_portsc_addr(xhci, i);
            val = xhci_readl(xhci, portsc);
            if (!(val & PORTSC_PP)) xhci_writel(xhci, portsc, val | PORTSC_PP);
        }
    }

    return 0;
}

/* =========================================================================
 * hc_driver: interrupt handler
 * ========================================================================= */
static void xhci_irq(struct usb_hcd* hcd)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    u32 usbsts;
    u32 iman;
    usbsts = xhci_readl(xhci, &xhci->op_regs->usbsts);

    /* Clear interrupt status bits if EINT is set (write-1-to-clear) */
    if (usbsts & STS_EINT) {
        xhci_writel(xhci, &xhci->op_regs->usbsts, STS_EINT | STS_PCD);
    }

    /* Handle host controller error */
    if (usbsts & STS_HCE) {
        printl("xhci: Host Controller Error! usbsts=0x%x\n", usbsts);
        goto out;
    }

    /* Handle port change detection */
    if (usbsts & STS_PCD) {
        xhci_handle_port_change(xhci);
    }

    /* Always process the event ring. With MSI-X, QEMU clears IMAN.IP
     * automatically after delivering the interrupt message, so IP
     * cannot be used as a pending-events indicator. The event ring's
     * cycle bit is the authoritative check for unconsumed events. */
    {
        volatile struct xhci_intr_regs* ir = &xhci->run_regs->irs[0];
        iman = xhci_readl(xhci, &ir->iman);

        xhci_handle_event(xhci, NULL);

        /* Clear IP if it's set (for INTx compatibility) */
        if (iman & IMAN_IP) {
            xhci_writel(xhci, &ir->iman, iman);
        }
    }

out:
    if (hcd->irq) irq_enable(&hcd->irq_hook);
}

static unsigned int xhci_endpoint_dci(struct usb_host_endpoint* ep)
{
    unsigned int epnum = usb_endpoint_num(&ep->desc);
    int is_in = ep->desc.bEndpointAddress & USB_DIR_IN;

    if (usb_endpoint_xfer_control(&ep->desc)) return 1;
    return epnum * 2 + (is_in ? 1 : 0);
}

static u32 xhci_endpoint_interval(struct usb_device* udev,
                                  struct usb_host_endpoint* ep)
{
    u32 interval = ep->desc.bInterval;
    u32 microframes;
    u32 encoded = 0;

    if (!interval) return 0;

    if (udev->speed == USB_SPEED_HIGH || udev->speed >= USB_SPEED_SUPER) {
        encoded = interval - 1;
    } else {
        microframes = interval * 8;
        while ((1U << encoded) < microframes && encoded < 15)
            encoded++;
    }

    if (encoded > 15) encoded = 15;
    return encoded;
}

static u16 xhci_endpoint_max_packet(struct usb_host_endpoint* ep)
{
    return le16_to_cpu(ep->desc.wMaxPacketSize) & 0x07ff;
}

static int xhci_configure_endpoint(struct xhci_hcd* xhci,
                                   struct usb_device* udev,
                                   struct usb_host_endpoint* ep)
{
    unsigned int slot_id = (unsigned int)(unsigned long)udev->hcpriv;
    unsigned int dci = xhci_endpoint_dci(ep);
    struct xhci_virt_device* vd;
    struct xhci_input_ctx* in_ctx;
    struct xhci_slot_ctx* slot_ctx;
    struct xhci_ep_ctx* ep_ctx;
    u16 max_packet;
    int ep_type;
    int retval;

    if (dci == 1) return 0;
    if (slot_id == 0 || slot_id > xhci->num_slots) return ENODEV;

    vd = xhci->devs[slot_id - 1];
    if (!vd) return ENODEV;

    if (vd->eps[dci].ring) {
        vd->eps[dci].ep = ep;
        return 0;
    }

    vd->eps[dci].ring = xhci_ring_alloc(1, 0);
    if (!vd->eps[dci].ring) return ENOMEM;
    vd->eps[dci].ep = ep;

    if (usb_endpoint_xfer_bulk(&ep->desc))
        ep_type = (ep->desc.bEndpointAddress & USB_DIR_IN) ? 6 : 2;
    else if (usb_endpoint_xfer_int(&ep->desc))
        ep_type = (ep->desc.bEndpointAddress & USB_DIR_IN) ? 7 : 3;
    else
        ep_type = (ep->desc.bEndpointAddress & USB_DIR_IN) ? 5 : 1;

    max_packet = xhci_endpoint_max_packet(ep);
    in_ctx = vd->in_ctx;
    memset(in_ctx, 0, xhci_input_ctx_size(xhci));

    in_ctx->ctrl.add_flags = cpu_to_le32(0x1 | (1U << dci));
    slot_ctx = (struct xhci_slot_ctx*)((char*)in_ctx + xhci->context_size);
    memcpy(slot_ctx, &vd->out_ctx->slot, sizeof(*slot_ctx));
    slot_ctx->info1 &= cpu_to_le32(~(0x1fU << 27));
    slot_ctx->info1 |= cpu_to_le32(dci << 27);

    ep_ctx = (struct xhci_ep_ctx*)((char*)in_ctx + xhci->context_size +
                                   dci * xhci->context_size);
    ep_ctx->info1 = 0;
    ep_ctx->info2 =
        cpu_to_le32((3 << 1) | (ep_type << 3) | ((u32)max_packet << 16));
    ep_ctx->deq = cpu_to_le64(vd->eps[dci].ring->first_seg->dma | 1);
    ep_ctx->tx_info = cpu_to_le32(max_packet);

    if (usb_endpoint_xfer_int(&ep->desc) || usb_endpoint_xfer_isoc(&ep->desc)) {
        ep_ctx->info1 |= cpu_to_le32(xhci_endpoint_interval(udev, ep) << 16);
    }

    retval = xhci_cmd_configure_ep(xhci, vd->in_ctx_dma, slot_id, 0);
    if (!retval) retval = xhci_wait_cmd_completion(xhci);
    if (retval) {
        xhci_ring_free(vd->eps[dci].ring);
        vd->eps[dci].ring = NULL;
        vd->eps[dci].ep = NULL;
        printl("xhci: Configure Endpoint failed (%d)\n", retval);
        return retval;
    }

    return 0;
}

/* =========================================================================
 * hc_driver: URB enqueue
 * ========================================================================= */
static int xhci_urb_enqueue(struct usb_hcd* hcd, struct urb* urb)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    struct usb_device* udev = urb->dev;
    unsigned int slot_id = (unsigned int)(unsigned long)udev->hcpriv;
    struct xhci_virt_device* vd;
    unsigned int dci;
    int ep_num, is_in;
    int retval;

    if (slot_id == 0 || slot_id > xhci->num_slots) return ENODEV;

    ep_num = usb_pipeendpoint(urb->pipe);
    is_in = usb_pipein(urb->pipe);
    dci = (ep_num == 0) ? 1 : ep_num * 2 + (is_in ? 1 : 0);

    if (!urb->ep) urb->ep = usb_pipe_endpoint(udev, urb->pipe);
    if (!urb->ep) return ENODEV;

    vd = xhci->devs[slot_id - 1];
    if (!vd || !vd->eps[dci].ring) return ENODEV;

    retval = usb_hcd_link_urb_to_ep(hcd, urb);
    if (retval) return retval;

    retval = xhci_queue_urb(xhci, urb, slot_id, dci);
    if (retval) {
        usb_hcd_unlink_urb_from_ep(hcd, urb);
        return retval;
    }

    xhci_ring_ep_db(xhci, slot_id, dci);
    return 0;
}

/* =========================================================================
 * hc_driver: endpoint management
 * ========================================================================= */
static void xhci_disable_endpoint(struct usb_hcd* hcd,
                                  struct usb_host_endpoint* ep)
{
    /* Nothing for now */
}

static void xhci_reset_endpoint(struct usb_hcd* hcd, struct usb_device* udev,
                                struct usb_host_endpoint* ep)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    unsigned int slot_id = (unsigned int)(unsigned long)udev->hcpriv;
    unsigned int dci = xhci_endpoint_dci(ep);
    struct xhci_virt_device* vd;
    int retval;

    if (dci == 1) return;
    if (slot_id == 0 || slot_id > xhci->num_slots) return;

    vd = xhci->devs[slot_id - 1];
    if (!vd) return;
    /* The recovery command chain owns this endpoint until Set Dequeue
     * completes.  Do not issue a competing reset or restart it early. */
    if (vd->eps[dci].recovery_state) return;

    if (!vd->eps[dci].ring) {
        xhci_configure_endpoint(xhci, udev, ep);
        return;
    }

    retval = xhci_cmd_reset_ep(xhci, slot_id, dci);
    if (!retval) retval = xhci_wait_cmd_completion(xhci);
    if (retval) printl("xhci: Reset Endpoint failed (%d)\n", retval);
}

/* =========================================================================
 * Input context building helpers
 * ========================================================================= */

/* Map USB speed to xHCI slot context speed value */
static u32 xhci_usb_speed_to_slot(enum usb_device_speed speed)
{
    switch (speed) {
    case USB_SPEED_FULL:
        return 1;
    case USB_SPEED_LOW:
        return 2;
    case USB_SPEED_HIGH:
        return 3;
    case USB_SPEED_SUPER:
        return 4;
    case USB_SPEED_SUPER_PLUS:
        return 4;
    default:
        return 1;
    }
}

/* Build the input context for Address Device command.
 * Sets up slot context and ep0 context. */
static void xhci_build_address_ctx(struct xhci_hcd* xhci,
                                   struct xhci_virt_device* virt_dev,
                                   struct usb_device* udev)
{
    struct xhci_input_ctx* in_ctx = virt_dev->in_ctx;
    struct xhci_slot_ctx* slot_ctx;
    struct xhci_ep_ctx* ep0_ctx;
    u16 max_packet = le16_to_cpu(udev->ep0.desc.wMaxPacketSize);
    unsigned int ctx_offset;

    memset(in_ctx, 0, xhci_input_ctx_size(xhci));

    /* Input control context: add slot (context 0) and ep0 (context 1) */
    in_ctx->ctrl.add_flags = cpu_to_le32(0x3); /* bits 0 and 1 */

    /* Slot context is the first device context after the control context */
    ctx_offset = xhci->context_size; /* skip input control context */
    slot_ctx = (struct xhci_slot_ctx*)((char*)in_ctx + ctx_offset);

    /* DW0: route string = 0 for root hub devices.
     * Note: QEMU's xhci_lookup_uport reads nibbles from DW0 starting at
     * bit 0 (including the speed field) for port path matching, so keep
     * bits [3:0] = 0 to terminate the path lookup immediately. */
    slot_ctx->info1 =
        cpu_to_le32((xhci_usb_speed_to_slot(udev->speed) << 20) | (1U << 27));
    slot_ctx->info2 = cpu_to_le32(udev->portnum << 16);
    slot_ctx->tt_info = 0;

    /* ep0 context follows slot context */
    ep0_ctx = (struct xhci_ep_ctx*)((char*)slot_ctx + xhci->context_size);

    /* DW0: EP type = control (4), max burst = 0
     * DW1: CErr=3, Max Packet Size in bits [31:16] */
    ep0_ctx->info1 = 0;
    ep0_ctx->info2 = cpu_to_le32((3 << 1) | (4 << 3) | ((u32)max_packet << 16));
    ep0_ctx->deq = cpu_to_le64(virt_dev->eps[1].ring->first_seg->dma | 1);
    ep0_ctx->tx_info = cpu_to_le32(8);
}

/* =========================================================================
 * hc_driver: device management
 * ========================================================================= */

static int xhci_enable_device(struct usb_hcd* hcd, struct usb_device* udev)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    struct xhci_virt_device* virt_dev;
    unsigned int slot_id;
    int retval;

    if (udev->hcpriv) return 0;

    retval = xhci_cmd_enable_slot(xhci, &slot_id);
    if (retval) {
        printl("xhci: Enable Slot failed (%d)\n", retval);
        return retval;
    }

    retval = xhci_alloc_dev(xhci, udev, slot_id);
    if (retval) return retval;

    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) return ENODEV;

    xhci_build_address_ctx(xhci, virt_dev, udev);

    retval = xhci_queue_command(xhci, (u32)(virt_dev->in_ctx_dma & 0xFFFFFFFF),
                                (u32)(virt_dev->in_ctx_dma >> 32), 0,
                                TRB_TYPE(TRB_ADDRESS_DEV) | TRB_SLOT(slot_id) |
                                    TRB_BSA);
    if (!retval) retval = xhci_wait_cmd_completion(xhci);
    if (retval) {
        printl("xhci: setup Address Device failed (%d)\n", retval);
        xhci_free_dev(xhci, udev);
        return retval;
    }

    virt_dev->enabled = 1;
    return 0;
}

static int xhci_address_device(struct usb_hcd* hcd, struct usb_device* udev)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    struct xhci_virt_device* virt_dev;
    unsigned int slot_id = (unsigned int)(unsigned long)udev->hcpriv;
    int retval;

    if (slot_id == 0 || slot_id > xhci->num_slots) return ENODEV;

    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) return ENODEV;

    xhci_build_address_ctx(xhci, virt_dev, udev);

    retval = xhci_cmd_address_dev(xhci, virt_dev->in_ctx_dma, slot_id);
    if (!retval) retval = xhci_wait_cmd_completion(xhci);
    if (retval) {
        printl("xhci: Address Device failed (%d)\n", retval);
        return retval;
    }

    virt_dev->enabled = 2;
    return 0;
}

static int xhci_reset_device(struct usb_hcd* hcd, struct usb_device* udev)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    unsigned int slot_id = (unsigned int)(unsigned long)udev->hcpriv;
    int retval;

    if (slot_id == 0 || slot_id > xhci->num_slots) return 0;
    if (!xhci->devs[slot_id - 1] || xhci->devs[slot_id - 1]->enabled < 2)
        return 0;

    retval = xhci_queue_command(xhci, 0, 0, 0,
                                TRB_TYPE(TRB_RESET_DEVICE) | TRB_SLOT(slot_id));
    if (!retval) retval = xhci_wait_cmd_completion(xhci);
    return retval;
}

/* =========================================================================
 * hc_driver: DMA mapping
 *
 * xHCI uses the scatter-gather approach for data buffers, but for
 * simplicity we can use the default linear DMA mapping provided by
 * the generic HCD layer. Override only if we need special handling.
 * ========================================================================= */

/* =========================================================================
 * hc_driver registration
 * ========================================================================= */
static const struct hc_driver xhci_hc_driver = {
    .description = hcd_name,
    .product_desc = "xHCI Host Controller",
    .hcd_priv_size = sizeof(struct xhci_hcd),
    .flags = HCD_USB3 | HCD_MEMORY,

    .irq = xhci_irq,

    .setup = xhci_setup,
    .start = xhci_start,

    .urb_enqueue = xhci_urb_enqueue,

    .disable_endpoint = xhci_disable_endpoint,
    .reset_endpoint = xhci_reset_endpoint,

    .hub_status_data = xhci_hub_status_data,
    .hub_control = xhci_hub_control,

    .reset_device = xhci_reset_device,
    .enable_device = xhci_enable_device,
    .address_device = xhci_address_device,
};

void xhci_init_driver(struct hc_driver* driver) { *driver = xhci_hc_driver; }
