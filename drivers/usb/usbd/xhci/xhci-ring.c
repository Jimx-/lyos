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
        /* Toggle the cycle state if the Link TRB has TC bit set */
        if (le32_to_cpu(trb->control) & TRB_TC) {
            ring->cycle_state ^= 1;
        }
        seg = seg->next;
        trb = seg->trbs;
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
    struct xhci_ring next = *ring;
    struct xhci_trb* trb = ring->enqueue;

    /* Check space before making even one TRB visible. */
    if (xhci_ring_advance_enqueue(&next) < 0) return ENOSPC;

    /* Copy the TRB template but set the cycle bit to match ring state */
    trb->parameter = trb_template->parameter;
    trb->status = trb_template->status;
    trb->control = cpu_to_le32(le32_to_cpu(trb_template->control) |
                               (ring->cycle_state & TRB_CYCLE));

    /* Memory barrier: ensure TRB is fully written before we advance */
    wmb();

    if (trb + 1 == &ring->enqueue_seg->trbs[TRBS_PER_SEGMENT - 1]) {
        struct xhci_trb* link = trb + 1;
        u32 control = le32_to_cpu(link->control) & ~(TRB_CYCLE | TRB_CHAIN);

        control |= ring->cycle_state | (le32_to_cpu(trb->control) & TRB_CHAIN);
        link->control = cpu_to_le32(control);
    }
    *ring = next;

    return 0;
}

struct xhci_urb_priv {
    struct xhci_segment* first_seg;
    struct xhci_trb* first;
    struct xhci_segment* end_seg;
    struct xhci_trb* end;
    unsigned int end_cycle;
    unsigned int num_trbs;
    int has_short_event;
    u32 short_actual_length;
};

/* Reserve the entire batch without modifying DMA memory.  With no sleeps
 * between reservation and publication, another worker cannot enqueue here.
 * Hold the first cycle bit invalid until every TRB and Link is ready. */
static int xhci_queue_trbs(struct xhci_ring* ring, struct xhci_trb* trbs,
                           unsigned int count, struct xhci_urb_priv* priv)
{
    struct xhci_ring cursor = *ring;
    struct xhci_trb* first = ring->enqueue;
    unsigned int first_cycle = ring->cycle_state;
    unsigned int i;

    for (i = 0; i < count; i++) {
        if (xhci_ring_advance_enqueue(&cursor) < 0) return ENOSPC;
    }

    priv->first_seg = ring->enqueue_seg;
    priv->first = first;
    priv->end_seg = cursor.enqueue_seg;
    priv->end = cursor.enqueue;
    priv->end_cycle = cursor.cycle_state;
    priv->num_trbs = count;

    cursor = *ring;
    for (i = 0; i < count; i++) {
        struct xhci_trb* trb = cursor.enqueue;
        u32 control = le32_to_cpu(trbs[i].control) & ~TRB_CYCLE;

        control |= cursor.cycle_state ^ (i == 0 ? TRB_CYCLE : 0);
        trb->parameter = trbs[i].parameter;
        trb->status = trbs[i].status;
        trb->control = cpu_to_le32(control);

        if (trb + 1 == &cursor.enqueue_seg->trbs[TRBS_PER_SEGMENT - 1]) {
            struct xhci_trb* link = trb + 1;
            u32 link_control = le32_to_cpu(link->control);

            link_control &= ~(TRB_CYCLE | TRB_CHAIN);
            link_control |= cursor.cycle_state | (control & TRB_CHAIN);
            link->control = cpu_to_le32(link_control);
        }
        xhci_ring_advance_enqueue(&cursor); /* reservation guarantees space */
    }

    *ring = cursor;
    wmb();
    first->control = cpu_to_le32((le32_to_cpu(first->control) & ~TRB_CYCLE) |
                                first_cycle);
    wmb();
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

    if (xhci->devs[slot_id - 1]->eps[dci].recovery_state) return;

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

/* Count exactly the same 64-KiB chunks used by the emission loop. */
static unsigned int xhci_data_trbs(phys_bytes dma, u32 len)
{
    return len ? ((u64)(dma & 0xffff) + len + 0xffff) >> 16 : 0;
}

int xhci_queue_urb(struct xhci_hcd* xhci, struct urb* urb, unsigned int slot_id,
                   unsigned int ep_index)
{
    struct xhci_virt_device* virt_dev;
    struct xhci_ring* ring;
    struct xhci_trb* trbs;
    struct xhci_urb_priv* priv;
    struct scatterlist* sg;
    int type = usb_pipetype(urb->pipe);
    int is_control = type == PIPE_CONTROL;
    int is_in = usb_pipein(urb->pipe);
    unsigned int maxp = usb_maxpacket(urb->dev, urb->pipe);
    u32 data_len = urb->transfer_buffer_length;
    u32 transferred = 0;
    u64 count = 0, mapped_len = 0;
    unsigned int index = 0;
    int i, retval;

    if (slot_id == 0 || slot_id > xhci->num_slots ||
        ep_index >= XHCI_MAX_ENDPOINTS)
        return ENODEV;
    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) return ENODEV;
    ring = virt_dev->eps[ep_index].ring;
    if (!ring) return ENODEV;
    if (virt_dev->eps[ep_index].recovery_state) return EBUSY;
    if (type == PIPE_ISOCHRONOUS) return ENOSYS;
    if (!maxp) return EINVAL;

    if (urb->num_mapped_sgs) {
        for_each_sg(urb->sg, sg, urb->num_mapped_sgs, i)
        {
            count += xhci_data_trbs(sg_dma_address(sg), sg_dma_len(sg));
            mapped_len += sg_dma_len(sg);
        }
        if (mapped_len != data_len) return EINVAL;
    } else {
        count = xhci_data_trbs(urb->transfer_dma, data_len);
    }
    count += is_control ? 2 : (data_len == 0);
    /* Leave one unused slot to distinguish full and empty rings. */
    if (count >= (u64)ring->num_segs * (TRBS_PER_SEGMENT - 1)) return ENOSPC;
    if (count > (size_t)-1 / sizeof(*trbs)) return ENOMEM;

    trbs = calloc((size_t)count, sizeof(*trbs));
    if (!trbs) return ENOMEM;
    priv = calloc(1, sizeof(*priv));
    if (!priv) {
        free(trbs);
        return ENOMEM;
    }

    if (is_control) {
        struct xhci_trb* trb = &trbs[index++];
        u32 trt = data_len == 0 ? 0 : (is_in ? 3 : 2);

        memcpy(&trb->parameter, urb->setup_packet, 8);
        trb->status = cpu_to_le32(8);
        trb->control = cpu_to_le32(TRB_TYPE(TRB_SETUP_STAGE) | TRB_IDT |
                                   (trt << 16));
    }

    sg = urb->num_mapped_sgs ? urb->sg : NULL;
    for (i = 0; i < (urb->num_mapped_sgs ? urb->num_mapped_sgs : 1); i++) {
        phys_bytes dma = sg ? sg_dma_address(sg) : urb->transfer_dma;
        u32 remaining = sg ? sg_dma_len(sg) : data_len;

        while (remaining) {
            struct xhci_trb* trb = &trbs[index++];
            u32 boundary = 0x10000 - (u32)(dma & 0xffff);
            u32 chunk = remaining < boundary ? remaining : boundary;
            u32 packets_left = 0;
            u32 control = TRB_ISP;

            if (is_control && transferred == 0) {
                control |= TRB_TYPE(TRB_DATA_STAGE);
                if (is_in) control |= TRB_DATA_DIR_IN;
            } else {
                control |= TRB_TYPE(TRB_NORMAL);
            }
            transferred += chunk;
            if (transferred < data_len) {
                control |= TRB_CHAIN;
                packets_left = ((u64)data_len + maxp - 1) / maxp -
                               transferred / maxp;
                if (packets_left > 31) packets_left = 31;
            } else if (!is_control) {
                control |= TRB_IOC;
            }
            trb->parameter = cpu_to_le64(dma);
            trb->status = cpu_to_le32(chunk | (packets_left << 17));
            trb->control = cpu_to_le32(control);
            dma += chunk;
            remaining -= chunk;
        }
        if (sg) sg = sg_next(sg);
    }

    if (is_control) {
        u32 control = TRB_TYPE(TRB_STATUS_STAGE) | TRB_IOC;

        /* No-data control requests have an IN status stage. */
        if (!data_len || !is_in) control |= TRB_DATA_DIR_IN;
        trbs[index++].control = cpu_to_le32(control);
    } else if (!data_len) {
        trbs[index++].control = cpu_to_le32(TRB_TYPE(TRB_NORMAL) | TRB_IOC);
    }

    urb->hc_priv = priv;
    retval = xhci_queue_trbs(ring, trbs, index, priv);
    if (retval) urb->hc_priv = NULL;
    free(trbs);
    if (retval) {
        free(priv);
        return retval;
    }
    return 0;
}

/* =========================================================================
 * Event ring processing
 *
 * The xHC writes event TRBs to the event ring. Software reads them by
 * advancing the ERDP (Event Ring Dequeue Pointer).
 * ========================================================================= */

#define XHCI_RECOVERY_STOP 1
#define XHCI_RECOVERY_RESET 2
#define XHCI_RECOVERY_DEQUEUE 3
#define XHCI_RECOVERY_FAILED 4

static void xhci_finish_transfer(struct xhci_hcd* xhci,
                                  struct xhci_virt_ep* ep, struct urb* urb,
                                  int status)
{
    struct xhci_urb_priv* priv = urb->hc_priv;

    ep->ring->dequeue_seg = priv->end_seg;
    ep->ring->dequeue = priv->end;
    urb->hc_priv = NULL;
    free(priv);
    usb_hcd_unlink_urb_from_ep(xhci_to_hcd(xhci), urb);
    usb_hcd_giveback_urb(xhci_to_hcd(xhci), urb, status);
}

/* Recovery runs through command events, never sleeping in the IRQ handler.
 * The failed URB stays mapped until hardware acknowledges Set Dequeue. */
static void xhci_queue_recovery(struct xhci_hcd* xhci, unsigned int slot,
                                 unsigned int dci, int state)
{
    struct xhci_virt_ep* ep = &xhci->devs[slot - 1]->eps[dci];
    struct xhci_ring* cmd = xhci->cmd_ring;
    struct xhci_urb_priv* priv = ep->recovery_urb->hc_priv;
    int retval;

    ep->recovery_state = state;
    ep->recovery_cmd = cmd->enqueue_seg->dma +
                      (cmd->enqueue - cmd->enqueue_seg->trbs) *
                          sizeof(struct xhci_trb);
    if (state == XHCI_RECOVERY_RESET)
        retval = xhci_cmd_reset_ep(xhci, slot, dci);
    else if (state == XHCI_RECOVERY_STOP)
        retval = xhci_cmd_stop_ep(xhci, slot, dci);
    else {
        phys_bytes dma = priv->end_seg->dma +
                         (priv->end - priv->end_seg->trbs) * sizeof(*priv->end);
        retval = xhci_cmd_set_tr_dequeue(xhci, slot, dci, dma, priv->end_cycle);
    }
    if (retval) {
        ep->recovery_state = XHCI_RECOVERY_FAILED;
        printl("xhci: recovery enqueue failed slot=%u ep=%u error=%d; retaining DMA mappings\n",
               slot, dci, retval);
    }
}

static void xhci_recovery_done(struct xhci_hcd* xhci, unsigned int slot,
                                unsigned int dci, u32 code)
{
    struct xhci_virt_ep* ep = &xhci->devs[slot - 1]->eps[dci];
    struct urb* urb = ep->recovery_urb;
    int status = ep->recovery_status;

    /* Stop can race a hardware halt; reset that halted endpoint instead. */
    if (code == COMP_CONTEXT_STATE_ERROR &&
        ep->recovery_state == XHCI_RECOVERY_STOP) {
        xhci_queue_recovery(xhci, slot, dci, XHCI_RECOVERY_RESET);
        return;
    }
    if (code != COMP_SUCCESS) {
        ep->recovery_state = XHCI_RECOVERY_FAILED;
        printl("xhci: recovery failed slot=%u ep=%u code=%u; retaining DMA mappings\n",
               slot, dci, code);
        return;
    }
    if (ep->recovery_state != XHCI_RECOVERY_DEQUEUE) {
        xhci_queue_recovery(xhci, slot, dci, XHCI_RECOVERY_DEQUEUE);
        return;
    }

    ep->recovery_state = 0;
    ep->recovery_urb = NULL;
    xhci_finish_transfer(xhci, ep, urb, status);
    if (!list_empty(&ep->ep->urb_list)) xhci_ring_ep_db(xhci, slot, dci);
}

/* Process a Transfer Event TRB */
static int xhci_event_offset(struct xhci_urb_priv* priv, phys_bytes event_dma,
                              u32 residual, u32* actual, unsigned int* type)
{
    struct xhci_segment* seg = priv->first_seg;
    struct xhci_trb* trb = priv->first;
    u32 offset = 0;
    unsigned int i;

    for (i = 0; i < priv->num_trbs; i++) {
        unsigned int trb_type = TRB_TYPE_GET(le32_to_cpu(trb->control));
        u32 len = (trb_type == TRB_NORMAL || trb_type == TRB_DATA_STAGE)
                      ? le32_to_cpu(trb->status) & 0x1ffff
                      : 0;
        phys_bytes dma = seg->dma + (trb - seg->trbs) * sizeof(*trb);

        if (dma == event_dma) {
            if (residual > len) return EINVAL;
            *actual = offset + len - residual;
            *type = trb_type;
            return 0;
        }
        offset += len;
        trb++;
        if (trb == &seg->trbs[TRBS_PER_SEGMENT - 1]) {
            seg = seg->next;
            trb = seg->trbs;
        }
    }
    return ENOENT;
}

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
    struct usb_host_endpoint* ep;
    struct urb* urb;
    struct xhci_urb_priv* priv;
    unsigned int trb_type;
    u32 actual;
    int urb_status = 0;

    if (slot_id == 0 || slot_id > xhci->num_slots ||
        ep_index >= XHCI_MAX_ENDPOINTS) {
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
    if (virt_dev->eps[ep_index].recovery_state) return;

    ep = virt_dev->eps[ep_index].ep;
    if (!ep || list_empty(&ep->urb_list)) {
        if (ep_index == 1 && comp_code == COMP_SUCCESS) return;
        printl("xhci: transfer event without urb slot=%d ep=%d code=%d\n",
               slot_id, ep_index, comp_code);
        return;
    }

    urb = list_first_entry(&ep->urb_list, struct urb, urb_list);
    priv = urb->hc_priv;
    if (!priv) return;
    /* A short event's residue belongs to the referenced TRB, not the
     * entire URB.  Also reject stale events for an already completed TD. */
    if (xhci_event_offset(priv, le64_to_cpu(event->parameter),
                          comp_code == COMP_SHORT_PACKET ? transfer_len : 0,
                          &actual, &trb_type))
        return;

    if (usb_pipecontrol(urb->pipe) && comp_code == COMP_SHORT_PACKET) {
        priv->has_short_event = 1;
        priv->short_actual_length = actual;
        return;
    }
    if (usb_pipecontrol(urb->pipe) && comp_code == COMP_SUCCESS &&
        trb_type != TRB_STATUS_STAGE)
        return;

    /* Calculate actual transfer length */
    if (comp_code == COMP_SUCCESS || comp_code == COMP_SHORT_PACKET) {
        urb->actual_length = priv->has_short_event ? priv->short_actual_length
                                                  : actual;
    } else {
        urb->actual_length = priv->has_short_event ? priv->short_actual_length : 0;
        urb_status = xhci_comp_to_errno(comp_code);
        virt_dev->eps[ep_index].recovery_urb = urb;
        virt_dev->eps[ep_index].recovery_status = urb_status;
        xhci_queue_recovery(xhci, slot_id, ep_index,
                            comp_code == COMP_STALL_ERROR ||
                                    comp_code == COMP_BABBLE_DETECTED ||
                                    comp_code == COMP_TRANSACTION_ERROR
                                ? XHCI_RECOVERY_RESET : XHCI_RECOVERY_STOP);
        return;
    }

    xhci_finish_transfer(xhci, &virt_dev->eps[ep_index], urb, urb_status);
}

/* Process a Command Completion Event TRB */
static void xhci_handle_cmd_completion(struct xhci_hcd* xhci,
                                       struct xhci_trb* event)
{
    u32 status = le32_to_cpu(event->status);
    u32 control = le32_to_cpu(event->control);
    unsigned int slot_id = control >> 24;
    u32 comp_code = GET_COMP_CODE(status);
    phys_bytes dma = le64_to_cpu(event->parameter);
    struct xhci_ring* ring = xhci->cmd_ring;
    struct xhci_segment* seg = ring->first_seg;
    unsigned int i;

    /* Reclaim command space, including recovery commands, before queuing
     * the next step.  Command completions arrive in ring order. */
    for (i = 0; i < ring->num_segs; i++, seg = seg->next) {
        if (dma >= seg->dma &&
            dma - seg->dma < (TRBS_PER_SEGMENT - 1) * sizeof(struct xhci_trb)) {
            struct xhci_trb* trb = &seg->trbs[(dma - seg->dma) / sizeof(*trb)];
            u32 cmd_control = le32_to_cpu(trb->control);
            unsigned int cmd_slot = cmd_control >> 24;
            unsigned int dci = (cmd_control >> 16) & 0x1f;

            trb++;
            if (trb == &seg->trbs[TRBS_PER_SEGMENT - 1]) {
                seg = seg->next;
                trb = seg->trbs;
            }
            ring->dequeue_seg = seg;
            ring->dequeue = trb;
            if (cmd_slot && cmd_slot <= xhci->num_slots &&
                xhci->devs[cmd_slot - 1]) {
                struct xhci_virt_ep* ep = &xhci->devs[cmd_slot - 1]->eps[dci];

                if (ep->recovery_urb && ep->recovery_state != XHCI_RECOVERY_FAILED &&
                    ep->recovery_cmd == dma) {
                    xhci_recovery_done(xhci, cmd_slot, dci, comp_code);
                    return; /* do not wake an unrelated command waiter */
                }
            }
            break;
        }
    }

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
