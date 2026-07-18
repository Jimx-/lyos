/*
 * xHCI memory management: allocation and initialization of rings,
 * device contexts, DCBAA, scratchpad buffers, and per-device tracking.
 */
#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <asm/barrier.h>

#include "hcd.h"
#include "xhci.h"

static size_t xhci_dma_size(size_t size)
{
    const size_t page_size = 4096;

    return (size + page_size - 1) & ~(page_size - 1);
}

static void* xhci_dma_alloc(size_t size, phys_bytes* dma)
{
    void* vaddr;
    size_t map_size = xhci_dma_size(size);
    int retval;

    vaddr =
        mmap(NULL, map_size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (vaddr == MAP_FAILED) return NULL;

    memset(vaddr, 0, map_size);

    retval = umap(SELF, UMT_VADDR, (vir_bytes)vaddr, map_size, dma);
    if (retval) {
        munmap(vaddr, map_size);
        return NULL;
    }

    return vaddr;
}

static void xhci_dma_free(void* vaddr, size_t size)
{
    if (vaddr) munmap(vaddr, xhci_dma_size(size));
}

/* Allocate a single ring segment (array of TRBs) with proper alignment */
static struct xhci_segment* xhci_segment_alloc(void)
{
    struct xhci_segment* seg;

    seg = malloc(sizeof(*seg));
    if (!seg) return NULL;
    memset(seg, 0, sizeof(*seg));

    seg->trbs = xhci_dma_alloc(TRB_SEGMENT_SIZE, &seg->dma);
    if (!seg->trbs) {
        free(seg);
        return NULL;
    }

    return seg;
}

static void xhci_segment_free(struct xhci_segment* seg)
{
    if (!seg) return;
    xhci_dma_free(seg->trbs, TRB_SEGMENT_SIZE);
    free(seg);
}

/* Link segments into a circular ring */
static void xhci_link_segments(struct xhci_segment* first,
                               struct xhci_segment* last, int is_event_ring)
{
    struct xhci_segment* prev = last;
    struct xhci_segment* cur = first;

    do {
        struct xhci_segment* next = cur->next ? cur->next : first;

        if (!is_event_ring) {
            /* Transfer/command rings: write a Link TRB at the end
             * of each segment to chain to the next segment. */
            struct xhci_trb* link = &cur->trbs[TRBS_PER_SEGMENT - 1];
            link->parameter = next->dma;
            link->status = 0;
            link->control = cpu_to_le32(TRB_TYPE(TRB_LINK) | TRB_CHAIN);
            /* The cycle bit (bit 0) of the Link TRB will be set by
             * the ring initialization code. */
        }

        cur->prev = prev;
        prev->next = cur;
        prev = cur;
        cur = next;
    } while (cur != first);
}

/* Allocate a ring with the given number of segments */
struct xhci_ring* xhci_ring_alloc(unsigned int num_segs, int is_event)
{
    struct xhci_ring* ring;
    struct xhci_segment* prev = NULL;
    unsigned int i;

    ring = malloc(sizeof(*ring));
    if (!ring) return NULL;
    memset(ring, 0, sizeof(*ring));

    ring->num_segs = num_segs;
    ring->cycle_state = 1; /* start with cycle bit = 1 */

    /* Allocate segments */
    for (i = 0; i < num_segs; i++) {
        struct xhci_segment* seg = xhci_segment_alloc();
        if (!seg) goto fail;

        if (prev) {
            prev->next = seg;
        } else {
            ring->first_seg = seg;
        }
        prev = seg;
    }

    /* Link segments into a ring */
    xhci_link_segments(ring->first_seg, prev, is_event);

    /* Initialize enqueue/dequeue pointers */
    ring->enqueue_seg = ring->first_seg;
    ring->enqueue = ring->first_seg->trbs;
    ring->dequeue_seg = ring->first_seg;
    ring->dequeue = ring->first_seg->trbs;

    /* For non-event rings, set the Link TRB cycle bit to match the
     * ring's initial cycle state (inverted, since the Link TRB is
     * toggled when the ring wraps). */
    if (!is_event) {
        struct xhci_segment* seg = ring->first_seg;
        do {
            struct xhci_trb* link = &seg->trbs[TRBS_PER_SEGMENT - 1];
            link->control |= cpu_to_le32(ring->cycle_state ^ TRB_CYCLE);
            seg = seg->next;
        } while (seg != ring->first_seg);
    }

    return ring;

fail: {
    struct xhci_segment* seg = ring->first_seg;
    while (seg) {
        struct xhci_segment* next = seg->next;
        xhci_segment_free(seg);
        seg = (next != ring->first_seg) ? next : NULL;
    }
}
    free(ring);
    return NULL;
}

void xhci_ring_free(struct xhci_ring* ring)
{
    struct xhci_segment* seg;
    struct xhci_segment* first;

    if (!ring) return;

    first = ring->first_seg;
    if (!first) {
        free(ring);
        return;
    }

    seg = first;
    do {
        struct xhci_segment* next = seg->next;
        xhci_segment_free(seg);
        seg = next;
    } while (seg && seg != first);

    free(ring);
}

/* Allocate a buffer with the given alignment and return its physical address.
 * Use page alignment to ensure physical contiguity within the allocation. */
static void* xhci_alloc_dma(size_t size, size_t align, phys_bytes* dma)
{
    (void)align;

    return xhci_dma_alloc(size, dma);
}

/* =========================================================================
 * DCBAA (Device Context Base Address Array)
 * ========================================================================= */
static int xhci_alloc_dcbaa(struct xhci_hcd* xhci)
{
    size_t size = sizeof(u64) * (xhci->num_slots + 1); /* slot 0 is reserved */

    xhci->dcbaa = xhci_alloc_dma(size, 64, &xhci->dcbaa_dma);
    if (!xhci->dcbaa) return ENOMEM;

    /* Slot 0 is reserved (NULL) - already zeroed by memset */
    return 0;
}

/* =========================================================================
 * Scratchpad buffers
 * ========================================================================= */
static int xhci_alloc_scratchpad(struct xhci_hcd* xhci)
{
    unsigned int i;
    u32 num_bufs = xhci->max_scratch_bufs;

    if (num_bufs == 0) return 0;

    /* Allocate the array of buffer pointers (must be 64-byte aligned) */
    xhci->scratchpad =
        xhci_alloc_dma(sizeof(u64) * num_bufs, 64, &xhci->scratchpad_dma);
    if (!xhci->scratchpad) return ENOMEM;

    /* Allocate the actual scratchpad buffers */
    xhci->scratchpad_bufs = calloc(num_bufs, sizeof(void*));
    if (!xhci->scratchpad_bufs) return ENOMEM;

    for (i = 0; i < num_bufs; i++) {
        phys_bytes buf_dma;
        void* buf = xhci_alloc_dma(xhci->page_size, xhci->page_size, &buf_dma);
        if (!buf) return ENOMEM;

        xhci->scratchpad_bufs[i] = buf;
        xhci->scratchpad[i] = buf_dma;
    }

    /* The DCBAA entry for slot 0 points to the scratchpad array */
    xhci->dcbaa[0] = xhci->scratchpad_dma;
    return 0;
}

/* =========================================================================
 * Event Ring Segment Table (ERST)
 * ========================================================================= */
static int xhci_alloc_erst(struct xhci_hcd* xhci)
{
    /* Use one segment for the event ring */
    unsigned int num_entries = 1;

    xhci->event_ring = xhci_ring_alloc(num_entries, 1);
    if (!xhci->event_ring) return ENOMEM;

    xhci->erst.entries = xhci_alloc_dma(
        sizeof(struct xhci_erst_entry) * num_entries, 64, &xhci->erst.dma);
    if (!xhci->erst.entries) return ENOMEM;

    xhci->erst.num_entries = num_entries;
    xhci->erst.ring = xhci->event_ring;

    /* Fill in the ERST entry */
    xhci->erst.entries[0].addr = xhci->event_ring->first_seg->dma;
    xhci->erst.entries[0].size = TRBS_PER_SEGMENT;

    return 0;
}

/* =========================================================================
 * Top-level memory init/cleanup
 * ========================================================================= */
int xhci_mem_init(struct xhci_hcd* xhci)
{
    int retval;

    /* Allocate DCBAA */
    retval = xhci_alloc_dcbaa(xhci);
    if (retval) return retval;

    /* Allocate scratchpad buffers */
    retval = xhci_alloc_scratchpad(xhci);
    if (retval) goto fail_dcbaa;

    /* Allocate command ring */
    xhci->cmd_ring = xhci_ring_alloc(1, 0);
    if (!xhci->cmd_ring) {
        retval = ENOMEM;
        goto fail_scratchpad;
    }

    /* Allocate event ring and ERST */
    retval = xhci_alloc_erst(xhci);
    if (retval) goto fail_cmd_ring;

    return 0;

fail_cmd_ring:
    xhci_ring_free(xhci->cmd_ring);
    xhci->cmd_ring = NULL;

fail_scratchpad: {
    unsigned int i;
    for (i = 0; i < xhci->max_scratch_bufs; i++) {
        if (xhci->scratchpad_bufs && xhci->scratchpad_bufs[i])
            xhci_dma_free(xhci->scratchpad_bufs[i], xhci->page_size);
    }
    free(xhci->scratchpad_bufs);
    xhci_dma_free(xhci->scratchpad, sizeof(u64) * xhci->max_scratch_bufs);
    xhci->scratchpad = NULL;
    xhci->scratchpad_bufs = NULL;
}

fail_dcbaa:
    xhci_dma_free(xhci->dcbaa, sizeof(u64) * (xhci->num_slots + 1));
    xhci->dcbaa = NULL;

    return retval;
}

void xhci_mem_cleanup(struct xhci_hcd* xhci)
{
    unsigned int i;

    /* Free all virtual devices */
    for (i = 0; i < xhci->num_slots; i++) {
        if (xhci->devs[i]) {
            xhci_free_dev(xhci, xhci->devs[i]->udev);
        }
    }

    /* Free event ring and ERST */
    if (xhci->erst.entries)
        xhci_dma_free(xhci->erst.entries,
                      sizeof(struct xhci_erst_entry) * xhci->erst.num_entries);
    xhci_ring_free(xhci->event_ring);
    xhci->event_ring = NULL;

    /* Free command ring */
    xhci_ring_free(xhci->cmd_ring);
    xhci->cmd_ring = NULL;

    /* Free scratchpad buffers */
    if (xhci->scratchpad_bufs) {
        for (i = 0; i < xhci->max_scratch_bufs; i++) {
            if (xhci->scratchpad_bufs[i])
                xhci_dma_free(xhci->scratchpad_bufs[i], xhci->page_size);
        }
        free(xhci->scratchpad_bufs);
        xhci->scratchpad_bufs = NULL;
    }
    xhci_dma_free(xhci->scratchpad, sizeof(u64) * xhci->max_scratch_bufs);
    xhci->scratchpad = NULL;

    /* Free DCBAA */
    xhci_dma_free(xhci->dcbaa, sizeof(u64) * (xhci->num_slots + 1));
    xhci->dcbaa = NULL;

    /* Free port array */
    free(xhci->ports);
    xhci->ports = NULL;
}

/* =========================================================================
 * Per-device allocation
 * ========================================================================= */
static size_t xhci_device_ctx_size(struct xhci_hcd* xhci)
{
    /* Slot context + 31 endpoint contexts, each of context_size */
    return (1 + XHCI_MAX_ENDPOINTS) * xhci->context_size;
}

size_t xhci_input_ctx_size(struct xhci_hcd* xhci)
{
    /* Input control context + device context */
    return xhci->context_size + xhci_device_ctx_size(xhci);
}

int xhci_alloc_dev(struct xhci_hcd* xhci, struct usb_device* udev,
                   unsigned int slot_id)
{
    struct xhci_virt_device* virt_dev;
    int retval;

    if (slot_id == 0 || slot_id > xhci->num_slots) return EINVAL;
    if (xhci->devs[slot_id - 1]) return EBUSY;

    virt_dev = malloc(sizeof(*virt_dev));
    if (!virt_dev) return ENOMEM;
    memset(virt_dev, 0, sizeof(*virt_dev));

    virt_dev->slot_id = slot_id;
    virt_dev->udev = udev;

    /* Allocate output device context */
    virt_dev->out_ctx =
        xhci_alloc_dma(xhci_device_ctx_size(xhci), 64, &virt_dev->out_ctx_dma);
    if (!virt_dev->out_ctx) {
        retval = ENOMEM;
        goto fail_free;
    }

    /* Allocate input context */
    virt_dev->in_ctx =
        xhci_alloc_dma(xhci_input_ctx_size(xhci), 64, &virt_dev->in_ctx_dma);
    if (!virt_dev->in_ctx) {
        retval = ENOMEM;
        goto fail_out_ctx;
    }

    /* Allocate ep0 transfer ring (DCI 1) */
    virt_dev->eps[1].ring = xhci_ring_alloc(1, 0);
    if (!virt_dev->eps[1].ring) {
        retval = ENOMEM;
        goto fail_in_ctx;
    }

    /* Set ep0 back-pointer for URB lookup */
    virt_dev->eps[1].ep = &udev->ep0;

    /* Register in DCBAA */
    xhci->dcbaa[slot_id] = virt_dev->out_ctx_dma;
    wmb();

    xhci->devs[slot_id - 1] = virt_dev;
    virt_dev->enabled = 1;

    /* Store slot_id in udev->hcpriv for later lookup */
    udev->hcpriv = (void*)(unsigned long)slot_id;

    return 0;

fail_in_ctx:
    xhci_dma_free(virt_dev->in_ctx, xhci_input_ctx_size(xhci));

fail_out_ctx:
    xhci_dma_free(virt_dev->out_ctx, xhci_device_ctx_size(xhci));

fail_free:
    free(virt_dev);
    return retval;
}

void xhci_free_dev(struct xhci_hcd* xhci, struct usb_device* udev)
{
    struct xhci_virt_device* virt_dev;
    unsigned int slot_id;
    unsigned int i;

    slot_id = (unsigned int)(unsigned long)udev->hcpriv;

    if (slot_id == 0 || slot_id > xhci->num_slots) return;
    virt_dev = xhci->devs[slot_id - 1];
    if (!virt_dev) return;

    /* Issue Disable Slot command */
    xhci_queue_command(xhci, 0, 0, 0,
                       TRB_TYPE(TRB_DISABLE_SLOT) | TRB_SLOT(slot_id));

    /* Free all endpoint rings */
    for (i = 0; i < XHCI_MAX_ENDPOINTS; i++) {
        if (virt_dev->eps[i].ring) {
            xhci_ring_free(virt_dev->eps[i].ring);
        }
    }

    /* Clear DCBAA entry */
    xhci->dcbaa[slot_id] = 0;

    /* Free contexts */
    xhci_dma_free(virt_dev->in_ctx, xhci_input_ctx_size(xhci));
    xhci_dma_free(virt_dev->out_ctx, xhci_device_ctx_size(xhci));

    xhci->devs[slot_id - 1] = NULL;
    udev->hcpriv = NULL;
    free(virt_dev);
}
