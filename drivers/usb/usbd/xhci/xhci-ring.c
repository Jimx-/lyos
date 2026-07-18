/*
 * xHCI ring management: TRB enqueue/dequeue, doorbell ringing,
 * command submission, and event ring processing.
 */
#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <asm/barrier.h>
#include <libasyncdriver/libasyncdriver.h>

#include "hcd.h"
#include "xhci.h"

/* =========================================================================
 * Ring enqueue
 *
 * Advance the enqueue pointer to the next TRB slot. Handles ring wraparound
 * by following the Link TRB at the end of each segment and toggling the
 * cycle state.
 *
 * Returns 0 on success, or -1 if the ring is full (no space).
 * ========================================================================= */
static int xhci_ring_advance_enqueue(struct xhci_ring* ring)
{
    struct xhci_segment* seg = ring->enqueue_seg;
    struct xhci_trb* trb = ring->enqueue;

    trb++;

    /* Check if we've reached the Link TRB at the end of this segment */
    if (TRB_TYPE_GET(le32_to_cpu(trb->control)) == TRB_LINK) {
        /* Follow the link to the next segment */
        seg = seg->next;
        trb = seg->trbs;

        /* Toggle the cycle state if the Link TRB has TC bit set */
        if (le32_to_cpu(ring->enqueue->control) & TRB_TC) {
            ring->cycle_state ^= 1;
        }
    }

    /* Check if the ring is full (enqueue catches up to dequeue) */
    if (seg == ring->dequeue_seg && trb == ring->dequeue) {
        return -1; /* ring full */
    }

    ring->enqueue_seg = seg;
    ring->enqueue = trb;

    return 0;
}

/* Enqueue a single TRB onto a ring.
 * The caller fills in the TRB contents (parameter, status, and the type
 * portion of control). This function sets the cycle bit and advances
 * the enqueue pointer.
 *
 * Returns 0 on success, ENOSPC if the ring is full. */
int xhci_ring_enqueue(struct xhci_ring* ring, struct xhci_trb* trb_template)
{
    struct xhci_trb* trb = ring->enqueue;

    /* Copy the TRB template but set the cycle bit to match ring state */
    trb->parameter = trb_template->parameter;
    trb->status = trb_template->status;
    trb->control = cpu_to_le32(le32_to_cpu(trb_template->control) |
                               (ring->cycle_state & TRB_CYCLE));

    /* Memory barrier: ensure TRB is fully written before we advance */
    wmb();

    /* Advance the enqueue pointer */
    if (xhci_ring_advance_enqueue(ring) < 0) {
        return ENOSPC;
    }

    return 0;
}

/* =========================================================================
 * Doorbell ringing
 * ========================================================================= */
void xhci_ring_cmd_db(struct xhci_hcd* xhci)
{
    volatile u32* db = xhci_db_addr(xhci, 0); /* slot 0 = command ring */

    wmb();
    xhci_writel(xhci, db, DB_TARGET_HOST_COMMAND);
}

void xhci_ring_ep_db(struct xhci_hcd* xhci, unsigned int slot_id,
                     unsigned int dci)
{
    volatile u32* db = xhci_db_addr(xhci, slot_id);

    /* Doorbell target is the DCI */
    wmb();
    xhci_writel(xhci, db, dci);
}

/* =========================================================================
 * Command submission
 *
 * Queue a command TRB on the command ring and ring the command doorbell.
 * ========================================================================= */
int xhci_queue_command(struct xhci_hcd* xhci, u32 field1, u32 field2,
                       u32 field3, u32 field4)
{
    struct xhci_trb trb;
    int retval;

    trb.parameter = ((u64)field2 << 32) | field1;
    trb.status = field3;
    trb.control = cpu_to_le32(field4);

    retval = xhci_ring_enqueue(xhci->cmd_ring, &trb);
    if (retval) return retval;

    xhci_ring_cmd_db(xhci);
    return 0;
}

/* Wait for a command completion event. Called after issuing a command. */
int xhci_wait_cmd_completion(struct xhci_hcd* xhci)
{
    xhci->cmd_done = 0;
    xhci->cmd_wid = asyncdrv_worker_id();

    if (!xhci->cmd_done) {
        asyncdrv_sleep();
    }

    return xhci_comp_to_errno(xhci->cmd_status);
}

/* Issue an Enable Slot command and wait for completion */
int xhci_cmd_enable_slot(struct xhci_hcd* xhci, unsigned int* slot_id)
{
    int retval;

    retval = xhci_queue_command(xhci, 0, 0, 0, TRB_TYPE(TRB_ENABLE_SLOT));
    if (retval) return retval;

    retval = xhci_wait_cmd_completion(xhci);
    if (retval) return retval;

    *slot_id = xhci->cmd_slot_id;
    return 0;
}

/* Issue a Disable Slot command */
int xhci_cmd_disable_slot(struct xhci_hcd* xhci, unsigned int slot_id)
{
    return xhci_queue_command(xhci, 0, 0, 0,
                              TRB_TYPE(TRB_DISABLE_SLOT) | TRB_SLOT(slot_id));
}

/* Issue an Address Device command */
int xhci_cmd_address_dev(struct xhci_hcd* xhci, phys_bytes in_ctx_dma,
                         unsigned int slot_id)
{
    u32 field1 = (u32)(in_ctx_dma & 0xFFFFFFFF);
    u32 field2 = (u32)(in_ctx_dma >> 32);

    return xhci_queue_command(xhci, field1, field2, 0,
                              TRB_TYPE(TRB_ADDRESS_DEV) | TRB_SLOT(slot_id));
}

/* Issue a Configure Endpoint command */
int xhci_cmd_configure_ep(struct xhci_hcd* xhci, phys_bytes in_ctx_dma,
                          unsigned int slot_id, int config_change)
{
    u32 field1 = (u32)(in_ctx_dma & 0xFFFFFFFF);
    u32 field2 = (u32)(in_ctx_dma >> 32);
    u32 control = TRB_TYPE(TRB_CONFIGURE_ENDPOINT) | TRB_SLOT(slot_id);

    if (config_change) control |= TRB_DC; /* deconfigure flag */

    return xhci_queue_command(xhci, field1, field2, 0, control);
}

/* Issue a Stop Endpoint command */
int xhci_cmd_stop_ep(struct xhci_hcd* xhci, unsigned int slot_id,
                     unsigned int ep_index)
{
    return xhci_queue_command(xhci, 0, 0, 0,
                              TRB_TYPE(TRB_STOP_ENDPOINT) | TRB_EP(ep_index) |
                                  TRB_SLOT(slot_id));
}

/* Issue a Reset Endpoint command */
int xhci_cmd_reset_ep(struct xhci_hcd* xhci, unsigned int slot_id,
                      unsigned int ep_index)
{
    return xhci_queue_command(xhci, 0, 0, 0,
                              TRB_TYPE(TRB_RESET_ENDPOINT) | TRB_EP(ep_index) |
                                  TRB_SLOT(slot_id));
}

/* Issue a Set TR Dequeue Pointer command */
int xhci_cmd_set_tr_dequeue(struct xhci_hcd* xhci, unsigned int slot_id,
                            unsigned int ep_index, phys_bytes dequeue_addr,
                            unsigned int cycle_state)
{
    u32 field1 = (u32)(dequeue_addr & 0xFFFFFFFF) | (cycle_state & 0x1);
    u32 field2 = (u32)(dequeue_addr >> 32);

    return xhci_queue_command(xhci, field1, field2, 0,
                              TRB_TYPE(TRB_SET_TR_DEQUEUE) | TRB_EP(ep_index) |
                                  TRB_SLOT(slot_id));
}

/* =========================================================================
 * URB queuing
 *
 * Convert a URB into one or more TRBs on the endpoint's transfer ring.
 *
 * For control transfers: SETUP + DATA (optional) + STATUS TRBs
 * For bulk transfers: one or more Normal TRBs (chained if needed)
 * For interrupt transfers: one Normal TRB
 * For isochronous transfers: one Isoch TRB per packet (TODO)
 * ========================================================================= */

int xhci_queue_urb(struct xhci_hcd* xhci, struct urb* urb, unsigned int slot_id,
                   unsigned int ep_index)
{
    struct xhci_virt_device* virt_dev;
    struct xhci_ring* ring;
    struct xhci_trb trb;
    int type = usb_pipetype(urb->pipe);
    int is_in = usb_pipein(urb->pipe);
    phys_bytes data_phys = urb->transfer_phys;
    u32 data_len = urb->transfer_buffer_length;
    int retval;

    if (slot_id == 0 || slot_id > xhci->num_slots) return ENODEV;
    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) return ENODEV;

    ring = virt_dev->eps[ep_index].ring;
    if (!ring) return ENODEV;

    virt_dev->eps[ep_index].has_short_event = 0;
    urb->hc_priv =
        (void*)(unsigned long)ep_index; /* remember ep for completion */

    switch (type) {
    case PIPE_CONTROL: {
        u32 trt;

        /* TRT (Transfer Type, bits [17:16]):
         *   0 = No Data, 2 = IN Data, 3 = OUT Data */
        if (data_len == 0)
            trt = 0;
        else if (is_in)
            trt = 2;
        else
            trt = 3;

        /* Stage 1: SETUP — IDT carries the 8-byte SETUP packet inline */
        memset(&trb, 0, sizeof(trb));
        memcpy(&trb.parameter, urb->setup_packet, 8);
        trb.status = 8;
        trb.control =
            cpu_to_le32(TRB_TYPE(TRB_SETUP_STAGE) | TRB_IDT | (trt << 16));
        retval = xhci_ring_enqueue(ring, &trb);
        if (retval) return retval;

        /* Stage 2: DATA (optional) */
        if (data_len > 0) {
            memset(&trb, 0, sizeof(trb));
            trb.parameter = data_phys;
            trb.status = data_len;
            trb.control = cpu_to_le32(TRB_TYPE(TRB_DATA_STAGE) | TRB_ISP);
            if (is_in) trb.control |= cpu_to_le32(TRB_DATA_DIR_IN);
            retval = xhci_ring_enqueue(ring, &trb);
            if (retval) return retval;
        }

        /* Stage 3: STATUS — IOC generates the completion event */
        memset(&trb, 0, sizeof(trb));
        trb.control = cpu_to_le32(TRB_TYPE(TRB_STATUS_STAGE) | TRB_IOC);
        if (data_len == 0 || is_in)
            ; /* STATUS direction is OUT */
        else
            trb.control |=
                cpu_to_le32(TRB_DATA_DIR_IN); /* STATUS IN after OUT data */
        retval = xhci_ring_enqueue(ring, &trb);
        if (retval) return retval;
        break;
    }

    case PIPE_BULK:
    case PIPE_INTERRUPT: {
        /* Queue as one or more chained Normal TRBs.
         * xHC supports up to 64KB per TRB (TD size field is 17 bits
         * for the remainder, but the Transfer Buffer Pointer can span
         * a full 64KB segment). We split into 16KB chunks for safety. */
        u32 remaining = data_len;
        phys_bytes cur_phys = data_phys;

        if (data_len == 0) {
            /* Zero-length transfer - queue a single No-Op style TRB */
            memset(&trb, 0, sizeof(trb));
            trb.control = cpu_to_le32(TRB_TYPE(TRB_NORMAL) | TRB_IOC);
            retval = xhci_ring_enqueue(ring, &trb);
            if (retval) return retval;
            break;
        }

        while (remaining > 0) {
            u32 chunk = remaining > 0x4000 ? 0x4000 : remaining;
            int is_last = (chunk == remaining);

            memset(&trb, 0, sizeof(trb));
            trb.parameter = cur_phys;
            trb.status = chunk;
            trb.control = cpu_to_le32(TRB_TYPE(TRB_NORMAL) | TRB_ISP);
            if (!is_last)
                trb.control |= cpu_to_le32(TRB_CHAIN);
            else
                trb.control |= cpu_to_le32(TRB_IOC);

            retval = xhci_ring_enqueue(ring, &trb);
            if (retval) return retval;

            cur_phys += chunk;
            remaining -= chunk;
        }
        break;
    }

    case PIPE_ISOCHRONOUS:
        /* TODO: queue Isoch TRBs with proper frame scheduling.
         * Each ISO packet needs its own TRB with the Start Frame,
         * TD size, and interrupt flags. */
        return ENOSYS;
    }

    return 0;
}

/* =========================================================================
 * Event ring processing
 *
 * The xHC writes event TRBs to the event ring. Software reads them by
 * advancing the ERDP (Event Ring Dequeue Pointer).
 * ========================================================================= */

/* Process a Transfer Event TRB */
static void xhci_handle_transfer_event(struct xhci_hcd* xhci,
                                       struct xhci_trb* event)
{
    u32 status = le32_to_cpu(event->status);
    u32 control = le32_to_cpu(event->control);
    unsigned int slot_id = control >> 24;
    unsigned int ep_index = (control >> 16) & 0x1f;
    u32 comp_code = GET_COMP_CODE(status);
    u32 transfer_len = status & 0xffffff;

    struct xhci_virt_device* virt_dev;
    struct usb_hcd* hcd = xhci_to_hcd(xhci);
    struct usb_host_endpoint* ep;
    struct urb* urb;
    int urb_status = 0;

    if (slot_id == 0 || slot_id > xhci->num_slots) {
        printl("xhci: bad transfer event slot=%d ep=%d code=%d\n", slot_id,
               ep_index, comp_code);
        return;
    }
    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) {
        printl("xhci: transfer event for missing slot=%d ep=%d code=%d\n",
               slot_id, ep_index, comp_code);
        return;
    }

    ep = virt_dev->eps[ep_index].ep;
    if (!ep || list_empty(&ep->urb_list)) {
        if (ep_index == 1 && comp_code == COMP_SUCCESS) return;
        printl("xhci: transfer event without urb slot=%d ep=%d code=%d\n",
               slot_id, ep_index, comp_code);
        return;
    }

    urb = list_first_entry(&ep->urb_list, struct urb, urb_list);

    if (ep_index == 1 && comp_code == COMP_SHORT_PACKET) {
        virt_dev->eps[ep_index].has_short_event = 1;
        virt_dev->eps[ep_index].short_actual_length =
            urb->transfer_buffer_length - transfer_len;
        return;
    }

    /* Calculate actual transfer length */
    if (virt_dev->eps[ep_index].has_short_event) {
        urb->actual_length = virt_dev->eps[ep_index].short_actual_length;
        virt_dev->eps[ep_index].has_short_event = 0;
    } else if (comp_code == COMP_SUCCESS) {
        urb->actual_length = urb->transfer_buffer_length;
    } else if (comp_code == COMP_SHORT_PACKET) {
        urb->actual_length = urb->transfer_buffer_length - transfer_len;
    } else {
        urb->actual_length = 0;
        urb_status = xhci_comp_to_errno(comp_code);
    }

    usb_hcd_unlink_urb_from_ep(hcd, urb);
    usb_hcd_giveback_urb(hcd, urb, urb_status);
}

/* Process a Command Completion Event TRB */
static void xhci_handle_cmd_completion(struct xhci_hcd* xhci,
                                       struct xhci_trb* event)
{
    u32 status = le32_to_cpu(event->status);
    u32 control = le32_to_cpu(event->control);
    unsigned int slot_id = control >> 24;
    u32 comp_code = GET_COMP_CODE(status);

    /* Store completion info for the waiting thread */
    xhci->cmd_status = comp_code;
    xhci->cmd_slot_id = slot_id;
    xhci->cmd_param = le64_to_cpu(event->parameter);
    xhci->cmd_done = 1;

    /* Wake up the waiting worker thread */
    asyncdrv_wakeup(xhci->cmd_wid);
}

/* Process a Port Status Change Event TRB */
static void xhci_handle_port_event(struct xhci_hcd* xhci,
                                   struct xhci_trb* event)
{
    /* Port Status Change Events contain the port ID in the parameter
     * field (bits 24-31). The actual port status must be read from
     * the PORTSC register. */
    (void)event;

    /* Defer to the hub handler which will read PORTSC for all ports */
    xhci_handle_port_change(xhci);
}

/* Main event handler - called from xhci_irq().
 * Processes events from the event ring.
 *
 * When called with event==NULL, it processes ALL pending events on
 * the event ring. When called with a specific event, it processes
 * just that one (used internally by the loop).
 */
int xhci_handle_event(struct xhci_hcd* xhci, struct xhci_trb* event)
{
    struct xhci_ring* ring = xhci->event_ring;
    struct xhci_segment* seg;
    struct xhci_trb* trb;
    u32 cycle_bit;
    u32 trb_type;

    /* If a specific event was passed, process it directly */
    if (event) {
        trb_type = TRB_TYPE_GET(le32_to_cpu(event->control));
        switch (trb_type) {
        case TRB_TRANSFER_EVENT:
            xhci_handle_transfer_event(xhci, event);
            break;
        case TRB_COMMAND_COMPLETION_EVENT:
            xhci_handle_cmd_completion(xhci, event);
            break;
        case TRB_PORT_STATUS_CHANGE_EVENT:
            xhci_handle_port_event(xhci, event);
            break;
        case TRB_MFINDEX_WRAP_EVENT:
            /* Used for periodic scheduling; ignore for now */
            break;
        case TRB_HOST_CONTROLLER_EVENT:
            /* TODO: handle HC error events */
            break;
        default:
            break;
        }
        return 0;
    }

    /* Process all pending events on the ring */
    seg = ring->dequeue_seg;
    trb = ring->dequeue;
    cycle_bit = ring->cycle_state;

    while (1) {
        u32 control = le32_to_cpu(trb->control);

        /* Check if the xHC has written this TRB (cycle bit matches) */
        if ((control & TRB_CYCLE) != (cycle_bit & TRB_CYCLE))
            break; /* no more events */

        /* Process this event */
        xhci_handle_event(xhci, trb);

        /* Advance dequeue pointer */
        trb++;
        if (trb - seg->trbs >= TRBS_PER_SEGMENT) {
            /* Wrap to next segment (follow Link TRB) */
            seg = seg->next;
            trb = seg->trbs;
            /* Toggle cycle state for event rings */
            cycle_bit ^= 1;
        }
    }

    /* Update the ERDP to tell the xHC we've consumed these events */
    {
        volatile struct xhci_intr_regs* ir = &xhci->run_regs->irs[0];
        unsigned int offset = (unsigned int)((char*)trb - (char*)seg->trbs);
        u64 erdp = seg->dma + offset;

        erdp |= (1 << 3); /* EHB bit: clear event handler busy */
        xhci_writeq(xhci, (u64*)&ir->erdp, erdp);
    }

    ring->dequeue_seg = seg;
    ring->dequeue = trb;
    ring->cycle_state = cycle_bit;

    return 0;
}
